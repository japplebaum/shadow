/**
 * The Shadow Simulator
 *
 * Copyright (c) 2010-2012 Rob Jansen <jansen@cs.umn.edu>
 *
 * This file is part of Shadow.
 *
 * Shadow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shadow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Shadow.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "shd-torrent.h"

Torrent* torrent;

static in_addr_t torrent_resolveHostname(const gchar* hostname) {
	ShadowlibLogFunc log = torrent->shadowlib->log;
	in_addr_t addr = 0;

	/* get the address in network order */
	if(g_strncasecmp(hostname, "none", 4) == 0) {
		addr = htonl(INADDR_NONE);
	} else if(g_strncasecmp(hostname, "localhost", 9) == 0) {
		addr = htonl(INADDR_LOOPBACK);
	} else {
		struct addrinfo* info;
		if(getaddrinfo((gchar*) hostname, NULL, NULL, &info) != -1) {
			addr = ((struct sockaddr_in*)(info->ai_addr))->sin_addr.s_addr;
		} else {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "unable to create client: error in getaddrinfo");
		}
		freeaddrinfo(info);
	}

	return addr;
}

static void torrent_report(TorrentClient* tc, gchar* preamble) {
	if(tc != NULL && preamble != NULL) {
		ShadowlibLogFunc log = torrent->shadowlib->log;
		struct timespec now;
		struct timespec first_time;
		struct timespec curr_time;
		struct timespec block_first_time;
		struct timespec block_curr_time;
		clock_gettime(CLOCK_REALTIME, &now);

		/* first byte statistics */
		first_time.tv_sec = tc->download_first_byte.tv_sec - tc->download_start.tv_sec;
		first_time.tv_nsec = tc->download_first_byte.tv_nsec - tc->download_start.tv_nsec;
		while(first_time.tv_nsec < 0) {
			first_time.tv_sec--;
			first_time.tv_nsec += 1000000000;
		}

		/* current byte statistics */
		curr_time.tv_sec = now.tv_sec - tc->download_start.tv_sec;
		curr_time.tv_nsec = now.tv_nsec - tc->download_start.tv_nsec;
		while(curr_time.tv_nsec < 0) {
			curr_time.tv_sec--;
			curr_time.tv_nsec += 1000000000;
		}

		/* first byte statistics */
		block_first_time.tv_sec = tc->currentBlockTransfer->download_first_byte.tv_sec - tc->currentBlockTransfer->download_start.tv_sec;
		block_first_time.tv_nsec = tc->currentBlockTransfer->download_first_byte.tv_nsec - tc->currentBlockTransfer->download_start.tv_nsec;
		while(block_first_time.tv_nsec < 0) {
			block_first_time.tv_sec--;
			block_first_time.tv_nsec += 1000000000;
		}

		/* current byte statistics */
		block_curr_time.tv_sec = now.tv_sec - tc->currentBlockTransfer->download_start.tv_sec;
		block_curr_time.tv_nsec = now.tv_nsec - tc->currentBlockTransfer->download_start.tv_nsec;
		while(block_curr_time.tv_nsec < 0) {
			block_curr_time.tv_sec--;
			block_curr_time.tv_nsec += 1000000000;
		}


		log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "%s first byte in %lu.%.3d seconds, "
				"%d of %d DOWN and %d of %d UP in %lu.%.3d seconds, total %d of %d bytes [%d\%] in %lu.%.3d seconds (block %d of %d)",
						preamble,
						block_first_time.tv_sec, (gint)(block_first_time.tv_nsec / 1000000),
						tc->currentBlockTransfer->downBytesTransfered, tc->downBlockSize,
						tc->currentBlockTransfer->upBytesTransfered, tc->upBlockSize,
						block_curr_time.tv_sec, (gint)(block_curr_time.tv_nsec / 1000000),
						tc->totalBytesDown, tc->fileSize, (gint)(((gdouble)tc->totalBytesDown / (gdouble)tc->fileSize) * 100),
						curr_time.tv_sec, (gint)(curr_time.tv_nsec / 1000000),
						tc->blocksDownloaded, tc->numBlocks);
	}
}

Torrent**  torrent_init(Torrent* currentTorrent) {
	torrent = currentTorrent;
	return &torrent;
}

void torrent_new(int argc, char* argv[]) {
	ShadowlibLogFunc log = torrent->shadowlib->log;
	log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "torrent_new called");

	torrent->server = NULL;
	torrent->client = NULL;
	torrent->authority = NULL;
	clock_gettime(CLOCK_REALTIME, &torrent->lastReport);
	torrent->clientDone = 0;

	const gchar* USAGE = "Torrent USAGE: \n"
			"\t'authority port'\n"
			"\t'node authorityHostname authorityPort socksHostname socksPort serverPort fileSize [downBlockSize upBlockSize]'";
	if(argc < 3) {
		log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "%s", USAGE);
		return;
	}

	gchar *mode = argv[1];

	if(g_strcasecmp(mode, "node") == 0) {
		if(argc < 5) {
			log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "%s", USAGE);
			return;
		}
		gchar* authHostname = argv[2];
		gint authPort = atoi(argv[3]);
		gchar* socksHostname = argv[4];
		gint socksPort = atoi(argv[5]);
		gint serverPort = atoi(argv[6]);

		gint fileSize = 0;
		if(strstr(argv[7], "KB") != NULL) {
			fileSize = atoi(strtok(argv[7], "K")) * 1024;
		} else if(strstr(argv[7], "MB") != NULL) {
			fileSize = atoi(strtok(argv[7], "M")) * 1024 * 1024;
		} else {
			fileSize = atoi(argv[7]);
		}

		gint downBlockSize = 16*1024;
		gint upBlockSize = 16*1024;

		if(argc == 10) {
			if(strstr(argv[8], "KB") != NULL) {
				downBlockSize = atoi(strtok(argv[8], "K")) * 1024;
			} else if(strstr(argv[8], "MB") != NULL) {
				downBlockSize = atoi(strtok(argv[8], "M")) * 1024 * 1024;
			} else {
				downBlockSize = atoi(argv[8]);
			}

			if(strstr(argv[9], "KB") != NULL) {
				upBlockSize = atoi(strtok(argv[9], "K")) * 1024;
			} else if(strstr(argv[9], "MB") != NULL) {
				upBlockSize = atoi(strtok(argv[9], "M")) * 1024 * 1024;
			} else {
				upBlockSize = atoi(argv[9]);
			}
		}

		/* create an epoll to wait for I/O events */
		gint epolld = epoll_create(1);
		if(epolld == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "Error in epoll_create");
			close(epolld);
			epolld = 0;
		}

		/* start server to listen for connections */
		in_addr_t listenIP = INADDR_ANY;
		in_port_t listenPort = (in_port_t)serverPort;

		torrent->server = g_new0(TorrentServer, 1);
		// NOTE: since the up/down block sizes are in context of the client, we swap them for
		// the server since it's actually the reverse of what the client has
		if(torrentServer_start(torrent->server, epolld, htonl(listenIP), htons(listenPort), upBlockSize, downBlockSize) < 0) {
			log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "torrent server error, not started!");
			g_free(torrent->server);
			torrent->server = NULL;
			return;
		} else {
			log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "torrent server running on at %s:%u", inet_ntoa((struct in_addr){listenIP}), listenPort);
		}

		/* create an epoll to wait for I/O events */
		epolld = epoll_create(1);
		if(epolld == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "Error in epoll_create");
			close(epolld);
			epolld = 0;
		}

		/* start up client */
		in_addr_t authAddr = torrent_resolveHostname(authHostname);
		in_addr_t socksAddr = torrent_resolveHostname(socksHostname);
		torrent->client = g_new0(TorrentClient, 1);
		if(torrentClient_start(torrent->client, epolld, socksAddr, htons(socksPort), authAddr, htons(authPort), serverPort,
				fileSize, downBlockSize, upBlockSize) < 0) {
			log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "torrent client error, not started!");
			g_free(torrent->client);
			torrent->client = NULL;
			return;
		} else {
			//log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "torrent client running");
		}
	} else if(g_strcasecmp(mode, "authority") == 0) {
		gint authPort = atoi(argv[2]);

		/* create an epoll to wait for I/O events */
		gint epolld = epoll_create(1);
		if(epolld == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "Error in epoll_create");
			close(epolld);
			epolld = 0;
		}

		in_addr_t listenIP = INADDR_ANY;
		in_port_t listenPort = (in_port_t)authPort;

		torrent->authority = g_new0(TorrentAuthority, 1);
		if(torrentAuthority_start(torrent->authority, epolld, htonl(listenIP), htons(listenPort), 0) < 0) {
			log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "torrent authority error, not started!");
			g_free(torrent->authority);
			torrent->authority = NULL;
			return;
		} else {
			log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "torrent authority running on at %s:%u", inet_ntoa((struct in_addr){listenIP}), listenPort);
		}
	}
}

void torrent_activate() {
	ShadowlibLogFunc log = torrent->shadowlib->log;
	log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "torrent_activate called");

	if(torrent->server) {
		if(!torrent->server->epolld) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "server can't wait on epoll without epoll descriptor");
			return;
		}

		struct epoll_event events[10];
		int nfds = epoll_wait(torrent->server->epolld, events, 10, 0);
		if(nfds == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "error in server epoll_wait");
			return;
		}

		for(int i = 0; i < nfds; i++) {
			gint res = torrentServer_activate(torrent->server, events[i].data.fd, events[i].events);
			if(res < 0) {
				log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "activate returned %d", res);
			}
		}
	}

	if(torrent->client) {
		struct epoll_event events[10];
		int nfds = epoll_wait(torrent->client->epolld, events, 10, 0);
		if(nfds == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "error in client epoll_wait");
			return;
		}

		for(int i = 0; i < nfds; i++) {
			gint res = torrentClient_activate(torrent->client, events[i].data.fd, events[i].events);
			if(res < 0) {
				log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "activate returned %d", res);
			}

			if(!torrent->clientDone  && torrent->client->totalBytesDown > 0) {
				struct timespec now;
				clock_gettime(CLOCK_REALTIME, &now);
				if(res == TC_BLOCK_DOWNLOADED) {
					torrent->lastReport = now;
					torrent_report(torrent->client, "[client-block-complete]");
				} else if(now.tv_sec - torrent->lastReport.tv_sec > 1 && torrent->client->currentBlockTransfer != NULL &&
						  (torrent->client->currentBlockTransfer->downBytesTransfered > 0 ||
						   torrent->client->currentBlockTransfer->upBytesTransfered > 0)) {
					torrent->lastReport = now;
					torrent_report(torrent->client, "[client-block-progress]");
				}

				/* if all the blocks have been downloaded, report final statistics and shutdown the client */
				if(torrent->client->blocksDownloaded >= torrent->client->numBlocks) {
					torrent_report(torrent->client, "[client-complete]");
					clock_gettime(CLOCK_REALTIME, &(torrent->client->download_end));
					torrentClient_shutdown(torrent->client);
					torrent->clientDone = 1;
				}
			}
		}
	} else if(torrent->authority) {
		if(!torrent->authority->epolld) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "authority can't wait on epoll without epoll descriptor");
			return;
		}

		struct epoll_event events[10];
		int nfds = epoll_wait(torrent->authority->epolld, events, 10, 0);
		if(nfds == -1) {
			log(G_LOG_LEVEL_WARNING, __FUNCTION__, "error in server epoll_wait");
			return;
		}

		for(int i = 0; i < nfds; i++) {
			gint res = torrentAuthority_activate(torrent->authority, events[i].data.fd);
			if(res < 0) {
				log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "activate returned %d", res);
			}
		}
	}
}

void torrent_free() {
	if(torrent->client) {
		/* Shutdown the client then free the object */
		torrentClient_shutdown(torrent->client);
		g_free(torrent->client);
		torrent->client = NULL;
	}

	if(torrent->server) {
		/* Shutdown the server then free the object */
		torrentServer_shutdown(torrent->server);
		g_free(torrent->client);
		torrent->server = NULL;
	}

	if(torrent->authority) {
		/* Shutdown the client then free the object */
		torrentAuthority_shutdown(torrent->authority);
		g_free(torrent->authority);
		torrent->authority = NULL;
	}
}
