/*
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

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "shd-filetransfer.h"

/* my global structure to hold all variable, node-specific application state */
FileTransfer* ft;

void filetransfer_init(FileTransfer* existingFT) {
	/* set our pointer to the existing global struct */
	ft = existingFT;
}

static void _filetransfer_logCallback(enum service_filegetter_loglevel level, const gchar* message) {
	if(level == SFG_CRITICAL) {
		ft->shadowlib->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "%s", message);
	} else if(level == SFG_WARNING) {
		ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "%s", message);
	} else if(level == SFG_NOTICE) {
		ft->shadowlib->log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "%s", message);
	} else if(level == SFG_INFO) {
		ft->shadowlib->log(G_LOG_LEVEL_INFO, __FUNCTION__, "%s", message);
	} else if(level == SFG_DEBUG) {
		ft->shadowlib->log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "%s", message);
	} else {
		/* we dont care */
	}
}

static in_addr_t _filetransfer_HostnameCallback(const gchar* hostname) {
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
			ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "unable to create client: error in getaddrinfo");
		}
		freeaddrinfo(info);
	}

	return addr;
}

static void _filetransfer_wakeupCallback(gpointer data) {
	service_filegetter_activate((service_filegetter_tp) data, 0);
}

/* called from inner filegetter code when it wants to sleep for some seconds */
static void _filetransfer_sleepCallback(gpointer sfg, guint seconds) {
	/* schedule a callback from shadow to wakeup the filegetter */
	ft->shadowlib->createCallback(&_filetransfer_wakeupCallback, sfg, seconds*1000);
}

static gchar* _filetransfer_getHomePath(const gchar* path) {
	GString* sbuffer = g_string_new("");
	if(g_ascii_strncasecmp(path, "~", 1) == 0) {
		/* replace ~ with home directory */
		const gchar* home = g_get_home_dir();
		g_string_append_printf(sbuffer, "%s%s", home, path+1);
	} else {
		g_string_append_printf(sbuffer, "%s", path);
	}
	return g_string_free(sbuffer, FALSE);
}

/* create a new node using this plug-in */
void filetransfer_new(int argc, char* argv[]) {
	ft->shadowlib->log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "filetransfer_new called");

	ft->client = NULL;
	ft->server = NULL;

	const gchar* USAGE = "\nFiletransfer usage:\n"
			"\t'server serverListenPort pathToDocRoot'\n"
			"\t'client single fileServerHostname fileServerPort socksServerHostname(or 'none') socksServerPort nDownloads pathToFile'\n"
			"\t'client multi pathToDownloadSpec socksServerHostname(or 'none') socksServerPort pathToThinktimeCDF(or 'none') secondsRunTime(or '-1') [nDownloads(or '-1')]'\n";
	if(argc < 2) goto printUsage;

	/* parse command line args, first is program name */
	gchar* mode = argv[1];

	/* create an epoll so we can wait for IO events */
	gint epolld = epoll_create(1);
	if(epolld == -1) {
		ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "Error in server epoll_create");
		close(epolld);
		epolld = 0;
	}

	if(g_strcasecmp(mode, "client") == 0) {
		/* check client args */
		if(argc < 3) goto printUsage;

		ft->client = g_new0(service_filegetter_t, 1);

		gchar* clientMode = argv[2];
		gint sockd = -1;

		if(g_strncasecmp(clientMode, "single", 6) == 0) {
			service_filegetter_single_args_t args;

			args.http_server.host = argv[3];
			args.http_server.port = argv[4];
			args.socks_proxy.host = argv[5];
			args.socks_proxy.port = argv[6];
			args.num_downloads = argv[7];
			args.filepath = _filetransfer_getHomePath(argv[8]);

			args.log_cb = &_filetransfer_logCallback;
			args.hostbyname_cb = &_filetransfer_HostnameCallback;
			args.sleep_cb = &_filetransfer_sleepCallback;

			enum filegetter_code result = service_filegetter_start_single(ft->client, &args, epolld, &sockd);

			if(result != FG_SUCCESS) {
				ft->shadowlib->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "fileclient error, not started!");
				g_free(ft->client);
				ft->client = NULL;
			}

			g_free(args.filepath);
		} else if(g_strncasecmp(clientMode, "multi", 5) == 0) {
			service_filegetter_multi_args_t args;
			memset(&args, 0, sizeof(service_filegetter_multi_args_t));

			args.server_specification_filepath = _filetransfer_getHomePath(argv[3]);
			args.socks_proxy.host = argv[4];
			args.socks_proxy.port = argv[5];
			args.thinktimes_cdf_filepath = _filetransfer_getHomePath(argv[6]);
			args.runtime_seconds = argv[7];

			if(argc > 8) {
				args.num_downloads = argv[8];
			}

			if(g_strncasecmp(args.thinktimes_cdf_filepath, "none", 4) == 0) {
				args.thinktimes_cdf_filepath = NULL;
			}

			args.log_cb = &_filetransfer_logCallback;
			args.hostbyname_cb = &_filetransfer_HostnameCallback;
			args.sleep_cb = &_filetransfer_sleepCallback;

			enum filegetter_code result = service_filegetter_start_multi(ft->client, &args, epolld, &sockd);

			if(result != FG_SUCCESS) {
				ft->shadowlib->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "fileclient error, not started!");
				g_free(ft->client);
				ft->client = NULL;
			}

			if(args.thinktimes_cdf_filepath)
				g_free(args.thinktimes_cdf_filepath);
			g_free(args.server_specification_filepath);
		} else {
			/* unknown client mode */
			g_free(ft->client);
			ft->client = NULL;
			goto printUsage;
		}

		/* successfull arguments */
		if(sockd >= 0) {
			service_filegetter_activate(ft->client, sockd);
		}
	} else if(g_strcasecmp(mode, "server") == 0) {
		/* check server args */
		if(argc < 4) goto printUsage;

		/* we are running a server */
		in_addr_t listenIP = INADDR_ANY;
		in_port_t listenPort = (in_port_t) atoi(argv[2]);
		gchar* docroot = _filetransfer_getHomePath(argv[3]);

		ft->server = g_new0(fileserver_t, 1);

		ft->shadowlib->log(G_LOG_LEVEL_INFO, __FUNCTION__, "serving '%s' on port %u", docroot, listenPort);
		enum fileserver_code res = fileserver_start(ft->server, epolld, htonl(listenIP), htons(listenPort), docroot, 1000);

		if(res == FS_SUCCESS) {
			ft->shadowlib->log(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "fileserver running on at %s:%u", inet_ntoa((struct in_addr){listenIP}), listenPort);
		} else {
			ft->shadowlib->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "fileserver error, not started!");
			g_free(ft->server);
			ft->server = NULL;
		}

		g_free(docroot);
	} else {
		/* not client or server... */
		goto printUsage;
	}

	return;
printUsage:
	ft->shadowlib->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__, (gchar*)USAGE);
	return;
}

void filetransfer_free() {
	ft->shadowlib->log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "filetransfer_free called");

	if(ft->client) {
		/* stop the client */
		service_filegetter_stop(ft->client);

		/* cleanup */
		g_free(ft->client);
		ft->client = NULL;
	}

	if(ft->server) {
		/* log statistics */
		ft->shadowlib->log(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"fileserver stats: %lu bytes in, %lu bytes out, %lu replies",
				ft->server->bytes_received, ft->server->bytes_sent,
				ft->server->replies_sent);

		/* shutdown fileserver */
		ft->shadowlib->log(G_LOG_LEVEL_INFO, __FUNCTION__, "shutting down fileserver");
		fileserver_shutdown(ft->server);

		/* cleanup */
		g_free(ft->server);
		ft->server = NULL;
	}
}

void filetransfer_activate() {
	ft->shadowlib->log(G_LOG_LEVEL_DEBUG, __FUNCTION__, "checking epoll for ready sockets");

	/* check clients epoll descriptor for events, and activate each ready socket */
	if(ft->client) {
		if(!ft->client->fg.epolld) {
			ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "client cant wait on epoll without epoll descriptor");
		} else {
			struct epoll_event events[10];
			int nfds = epoll_wait(ft->client->fg.epolld, events, 10, 0);
			if(nfds == -1) {
				ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "error in client epoll_wait");
			} else {
				/* finally, activate client for every socket thats ready */
				for(int i = 0; i < nfds; i++) {
					service_filegetter_activate(ft->client, events[i].data.fd);
				}
			}
		}
	}

	/* check servers epoll descriptor for events, and activate each ready socket */
	if(ft->server) {
		if(!ft->server->epolld) {
			ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "server cant wait on epoll without epoll descriptor");
		} else {
			struct epoll_event events[10];
			int nfds = epoll_wait(ft->server->epolld, events, 10, 0);
			if(nfds == -1) {
				ft->shadowlib->log(G_LOG_LEVEL_WARNING, __FUNCTION__, "error in server epoll_wait");
			} else {
				/* finally, activate server for every socket thats ready */
				for(int i = 0; i < nfds; i++) {
					enum fileserver_code result = fileserver_activate(ft->server, events[i].data.fd);
					ft->shadowlib->log(G_LOG_LEVEL_DEBUG, __FUNCTION__,
							"fileserver activation result: %s (%lu bytes in, %lu bytes out, %lu replies)",
							fileserver_codetoa(result),  ft->server->bytes_received,
							ft->server->bytes_sent, ft->server->replies_sent);
				}
			}
		}
	}
}
