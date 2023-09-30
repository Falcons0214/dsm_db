// #include <asm-generic/errno-base.h>
// #include <asm-generic/socket.h>
#include <complex.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../include/net.h"
#include "../../include/cmd.h"
#include "../../include/wrap.h"

#define __REPLY(sock_fd, msg_type, msg_value) \
		send(sock_fd, &msg_type, MSG_TYPE_SIZE, 0); \
		send(sock_fd, &msg_value, MSG_LENGTH_SIZE, 0);

/*
 * Structure for hook system information.
 */
static sys_args_s sysargs;

int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
	int				listenfd, n;
	const int		on = 1;
	struct addrinfo	hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;        /* let getaddrinfo() node type be suitable for bind() */
	hints.ai_family = AF_UNSPEC;        /* for any protocol */
	hints.ai_socktype = SOCK_STREAM;    /* for tcp */

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0) {
    }

	ressave = res;
	for(; res != NULL; res = res->ai_next) {
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)
			continue;		/* error, try next one */

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        }

        /*  
            SOL_SOCKET:     for the socket level.
            SO_REUSEADDR:   the socket level option, for ??
        */
		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break;			/* success */
		close(listenfd);    /* bind error, close and try next one */
	}

	if (!res) {	/* errno from final socket() or bind() */
    }

	if (listen(listenfd, LISTENQ) < 0) {
    }

	if (addrlenp)
		*addrlenp = res->ai_addrlen;	/* return size of protocol address */
	freeaddrinfo(ressave);
	return listenfd;
}

void connmg_init(conn_manager_s *connmg)
{
	for (int i = 0; i < MAX_CONNECTIONS; i ++)
		connmg->conn_table[i] = NULL;
	connmg->connection = 0;
	connmg->tesk_pool = tpool_create(32);
}

conn_info_s* conn_create(int fd)
{
	conn_info_s *conninfo = (conn_info_s*)malloc(sizeof(conn_info_s));
	if (!conninfo) {
		// err handler
	}
	conninfo->fd = fd;
	conninfo->flags = 0;
	return conninfo;
}

void conn_close(conn_info_s *conn)
{
	conn_manager_s *connmg = sysargs.connmg;

	if (epoll_ctl(connmg->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL) == -1) {
		perror("epoll_ctl: close");
		exit(EXIT_FAILURE);
	}
	
	connmg->conn_table[conn->fd - SYSRESERVFD] = NULL;
	connmg->connection --;
	close(conn->fd);
	free(conn);
}

void* exec(void *args)
{
	conn_info_s *conn = (conn_info_s*)args;
	conn_manager_s *connmg = sysargs.connmg;
	char msg_type, buf[MAX_CMDLEN];
	int cmd_number;
	if (!conn) return NULL;
	
	int cfd = conn->fd, msg_value;
	switch (recv(cfd, &msg_type, MSG_TYPE_SIZE, 0)) {
		case -1:
			printf("recv error: %s\n", strerror(errno));
			return NULL;
		case 0:
			if (CINFO_GET_CLOSE_BIT(conn->flags)) {
				conn_close(conn);
				printf("%d close\n", cfd);
				return NULL;
			}
			break;
		default:
			break;
	}
	recv(cfd, &msg_value, MSG_LENGTH_SIZE, 0);
	if (msg_type == MSG_TYPE_CMD)
		recv(cfd, buf, msg_value + 4, 0);

	if (msg_type == MSG_TYPE_CONNECT && msg_value == TYPE_CONNECT_EXIT)
		CINFO_CLOSE_UP(&(conn->flags));
	else {
		memcpy(&cmd_number, buf, sizeof(int));
		// Skip first separater sign.
		executer(&sysargs, conn, cmd_number, &buf[5], msg_value);
	}

	if (cfd > FD_RECYCLE_THRESHOLD) 
		for (int i = 0; i < MAX_CONNECTIONS; i ++)
			if (connmg->conn_table[i] && CINFO_GET_CLOSE_BIT(connmg->conn_table[i]->flags))
				conn_close(connmg->conn_table[i]);
	return NULL;
}

int db_active(disk_mg_s *dm, pool_mg_s *pm, conn_manager_s *connmg, char *port)
{
	struct epoll_event ev, events[MAX_CONNECTIONS];
	int listen_sock, conn_sock, nfds, epollfd, flags, msg_value;
	socklen_t addrlen;
	struct sockaddr_storage	connaddr;
	tpool_future_t future;
	char msg_type;

	/*
	 * Hook system args.
	 */
	sysargs.dm = dm;
	sysargs.pm = pm;
	sysargs.connmg = connmg;

	listen_sock = tcp_listen(NULL, port, NULL);
	// setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (void*)&buffer_len, buffer_len);

	/*
	 * Set listen socket to Non-blocking mode. 
	 */
	flags = fcntl(listen_sock, F_GETFL);
	fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK);

	epollfd = epoll_create1(0);
	connmg->epoll_fd = epollfd;
	
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		nfds = epoll_wait(epollfd, events, MAX_CONNECTIONS, -1);

		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		for (int n = 0; n < nfds; n ++) {
			int cfd = events[n].data.fd;
			/*
			 * If cfd equal listen socket, mean's we have new connection coming.
			 * For new connection, we need create corresponding structure(conn_info_s),
			 * for maintain connection.
			 * 
			 * If cfd not equal listen socket, mean's we have request from client,
			 * and we push this request in thread pool waiting queue.
			 */
			if (cfd == listen_sock) {
				conn_sock = accept(listen_sock, (struct sockaddr *)&connaddr, &addrlen);
				if (conn_sock == -1) {
					perror("accept");
					exit(EXIT_FAILURE);
				}else{
					if (cfd >= FDLIMIT) {
						msg_type = MSG_TYPE_CONNECT;
						msg_value = TYPE_CONNECT_FDEXD;
						__REPLY(conn_sock, msg_type, msg_value)
						close(conn_sock);
					}else{
						/*
						 * Set conn_sock to Non-blocking mode.
						 */
						flags = fcntl(conn_sock, F_GETFL);
						fcntl(conn_sock, F_SETFL, flags | O_NONBLOCK);

						ev.events = EPOLLIN | EPOLLET;
                       	ev.data.fd = conn_sock;

						if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
							perror("epoll_ctl: listen_sock");
							exit(EXIT_FAILURE);
						}

						conn_info_s *conn = conn_create(conn_sock);
						if (!conn) {
							// err handler, for conn create fail.
						}
						connmg->conn_table[conn_sock  - SYSRESERVFD] = conn;
						msg_type = MSG_TYPE_CONNECT;
						msg_value = TYPE_CONNECT_BIND;
						__REPLY(conn_sock, msg_type, msg_value)
					 	printf("BB fd: %d %d\n", cfd, conn_sock);
					}
				}
			}else{
				printf("AA fd: %d %d\n", cfd, n);
				future = tpool_apply(connmg->tesk_pool, exec, (void*)connmg->conn_table[cfd - SYSRESERVFD]);
				tpool_future_destroy(future);
			}
		}
	}

    return 0;
}

void __attr_type_parser(char *str)
{
	uint16_t len;
	char buf[128];
    for (int index = 0; str[index] != '\0';) {
        memcpy(&len, &str[index], 2);
        memset(buf, 0, 128);
        memcpy(buf, &str[index + 4], len);
        printf("--> attr name: %d %s\n", len, buf);
        printf("--> attr type: %d\n", *((uint16_t*)&str[index + 2]));
        index += (len + 4);
    }
}

void __attr_value_parser(char *str)
{
	for (int i = 0, h = 0;; i ++, h ++) {
		if (str[i] == '\0') {
			printf("--> %s\n", &str[i - h + ((i == h) ? 0 : 1)]);
			h = 0;
			if (str[i + 1] == '\0')
				break;
		}
	}
}

void executer(sys_args_s *sysargs, conn_info_s *conn_info, uint32_t command, char *buf, int msg_len)
{
	char reply_type, *argv[2];
	int i;

	reply_type = (IS_CONTENT_UP(command)) ? MSG_TYPE_CONTENT : MSG_TYPE_STATE;
	command = __SKIP_CSBIT(command);

	argv[0] = buf;
	argv[1] = NULL;
	for (i = 0; i < msg_len; i ++) {
		if (buf[i] == '|') {
			buf[i] = '\0';
			argv[1] = &buf[i + 1];
			i += 2;
			break;
		}
	}
	// printf("%d, %d, %d, %d\n", command, reply_type, msg_len, i);
	// printf("table name: %s, command: %d\n", argv[0], command);

	// if (command == CMD_G_CREATE || command == CMD_I_CREATE)
	// 	__attr_type_parser(argv[1]);
	// else if (command == CMD_G_INSERT || command == CMD_I_INSERT)
	//  	__attr_value_parser(argv[1]);

	simple_db_executer(sysargs, conn_info, argv, reply_type, command, msg_len - i);
	// send(conn_info->fd, reply, strlen(reply), 0);
	return;
}