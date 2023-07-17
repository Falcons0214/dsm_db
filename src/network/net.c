#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../include/net.h"
#include "../../include/cmd.h"

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
	return(listenfd);
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
	return conninfo;
}

void conn_close(conn_info_s *conn)
{
	conn_manager_s *connmg = sysargs.connmg;
	connmg->conn_table[conn->fd - SYSRESERVFD] = NULL;

	if (epoll_ctl(connmg->epoll_fd, EPOLL_CTL_ADD, conn->fd, NULL) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}	

	close(conn->fd);
	
	// do somehting need for close.
	connmg->connection --;
	free(conn);
}

void* exec(void *args)
{
	conn_info_s *conn = (conn_info_s*)args;
	conn_manager_s *connmg = sysargs.connmg;
	char cmd[MAX_CMDLEN];
	int re_by_recv, cfd = conn->fd;

	re_by_recv = recv(cfd, cmd, MAX_CMDLEN, 0);
	printf("--> from: %d, msg: %s\n", conn->fd, cmd);
	if (re_by_recv <= 0) {
		if (re_by_recv == -1) {
			// err handler
		}else
			conn_close(connmg->conn_table[cfd - SYSRESERVFD]);
	}else
		executer(&sysargs, conn, cmd);
	
	return NULL;
}

int db_active(disk_mg_s *dm, pool_mg_s *pm, conn_manager_s *connmg, char *port)
{
	struct epoll_event ev, events[MAX_CONNECTIONS];
	int listen_sock, conn_sock, nfds, epollfd, flags;
	socklen_t addrlen;
	struct sockaddr_storage	connaddr;
	tpool_future_t future;
	char *buf;

	sysargs.dm = dm;
	sysargs.pm = pm;
	sysargs.connmg = connmg;

	listen_sock = tcp_listen(NULL, port, NULL);
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
			if (cfd == listen_sock) {
				conn_sock = accept(listen_sock, (struct sockaddr *)&connaddr, &addrlen);
				if (conn_sock == -1) {
					perror("accept");
					exit(EXIT_FAILURE);
				}else{
					if (cfd >= FDLIMIT) {
						buf = "Server is busy, please connect again later ...";
						send(conn_sock, buf, strlen(buf), 0);
						close(conn_sock);
					}else{
						// set conn_sock NON_BLOCKING
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
						buf = "binding\n";
						send(conn_sock, buf, strlen(buf), 0);
					}
				}
			}else{
				future = tpool_apply(connmg->tesk_pool, exec, (void*)connmg->conn_table[cfd - SYSRESERVFD]);
				tpool_future_destroy(future);
			}
		}
	}

    return 0;
}
