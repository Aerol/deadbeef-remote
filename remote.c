/*

BSD License

Copyright (C) 2013 Henry Case <rectifier04@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

     1.    Redistributions of source code must retain the above copyright notice,
           this list of conditions and the following disclaimer.
     2.    Redistributions in binary form must reproduce the above copyright notice,
           this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/poll.h>

#include "remote.h"
#include <deadbeef/deadbeef.h>

#define trace(fmt,...)
#define BUF_SIZE 500

static DB_remote_plugin_t plugin;
static DB_functions_t *deadbeef;
static uintptr_t remote_mutex;
static intptr_t remote_tid; // thread id?
static int remote_stopthread;
int sfd; // Socket fd

/*
  Listen for UDP packets for actions to perform.
  Next track, prev track, play/pause, stop, etc.
 */

static void
perform_action (char buf) {
    printf ("char %d", buf);
    switch (buf) {
    case '1':
	action_play_cb (NULL, NULL);
	break;
    case '2':
	action_prev_cb (NULL, NULL);
	break;
    case '3':
	action_next_cb (NULL, NULL);
	break;
    case '4':
	action_stop_cb (NULL, NULL);
	break;
    case '5':
	action_toggle_pause_cb (NULL, NULL);
	break;
    default:
	break;
    }
}

static void
remote_listen (void) {
    // start listening for udp packets.
    struct addrinfo hints, *results;
    int status;

    memset(&hints, 0, sizeof hints); // Makes sure the struct is empty, thanks beej.

    hints.ai_family = AF_UNSPEC; // Use IPv4 or IPv6.
    hints.ai_socktype = SOCK_DGRAM; // Using UDP
    hints.ai_flags = AI_PASSIVE; // Don't need to know our local IP. Thanks Unix!
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    deadbeef->conf_lock();
    if ((status = getaddrinfo(deadbeef->conf_get_str_fast ("remote.listen",""),
			      deadbeef->conf_get_str_fast ("remote.port",""), &hints, &results)) != 0) {
	fprintf (stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    }
    deadbeef->conf_unlock();
    // Do stuff
    if ((sfd = socket (results->ai_family, results->ai_socktype, results->ai_protocol)) != 0) {
	trace ("couldn't create socket'");
    }
    if ((bind(sfd, results->ai_addr, results->ai_addrlen)) != 0) {
	trace ("couldn't bind'");
    }

    // Done with results
    freeaddrinfo (results);
}

static void
remote_thread (void *ha) {
    struct pollfd ufds[1];
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    char buf[BUF_SIZE];

    ufds[0].fd = sfd;
    ufds[0].events = POLLIN;

    // recvfrom and process messages.
    for (;;) {

    	if (remote_stopthread == 1) {
    	    deadbeef->mutex_unlock (remote_mutex);
    	    printf("stopping thread");
    	    return;
    	}
    	peer_addr_len = sizeof (struct sockaddr_storage);
	int f = poll (ufds, 1, 1000);
	if (f == -1) {
	    printf ("error occurred in poll()");
	} else if (f == 0) {
	    continue;
	} else {
	    nread = recvfrom (sfd, buf, BUF_SIZE, 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
	}

    	if (nread == -1) {
    	    continue;
    	}

    	// Do stuff with buf?
	if (buf[0] != 0) {
	    printf("buf: %x\n", buf[0]);
	    perform_action (buf[0]);
	    // We've read buf, we can clear it now.
	    buf[0] = 0;
	}
    }

    return;
}

static int
plugin_start (void) {
    // Start plugin (duh)
    // Setup UDP listener to do stuff when receiving special datagrams
    if (deadbeef->conf_get_int ("remote.enable", 0)) {
	remote_stopthread = 0;
	remote_mutex = deadbeef->mutex_create_nonrecursive ();
	remote_listen ();
	remote_tid = deadbeef->thread_start (remote_thread, NULL);
    }
    //
    return 0;
}

static int
plugin_stop (void) {
    // Stop listener and cleanup.
    if (remote_tid) {
	remote_stopthread = 1;
	trace ("waiting for thread to finish\n");
	deadbeef->thread_join (remote_tid);
	deadbeef->mutex_free (remote_mutex);
	close (sfd);
    }
    return 0;
}

int
action_play_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_PLAY_CURRENT, 0, 0, 0);
    return 0;
}

int
action_prev_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_PREV, 0, 0, 0);
    return 0;
}

int
action_next_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_NEXT, 0, 0, 0);
    return 0;
}

int
action_stop_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_STOP, 0, 0, 0);
    return 0;
}

int
action_toggle_pause_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_TOGGLE_PAUSE, 0, 0, 0);
    return 0;
}

int
action_play_pause_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    int state = deadbeef->get_output ()->state ();
    if (state == OUTPUT_STATE_PLAYING) {
        deadbeef->sendmessage (DB_EV_PAUSE, 0, 0, 0);
    }
    else {
        deadbeef->sendmessage (DB_EV_PLAY_CURRENT, 0, 0, 0);
    }
    return 0;
}

int
action_play_random_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->sendmessage (DB_EV_PLAY_RANDOM, 0, 0, 0);
    return 0;
}

int
action_seek_forward_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->playback_set_pos (deadbeef->playback_get_pos () + 5);
    return 0;
}

int
action_seek_backward_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->playback_set_pos (deadbeef->playback_get_pos () - 5);
    return 0;
}

int
action_volume_up_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->volume_set_db (deadbeef->volume_get_db () + 2);
    return 0;
}

int
action_volume_down_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    deadbeef->volume_set_db (deadbeef->volume_get_db () - 2);
    return 0;
}

int
action_toggle_stop_after_current_cb (struct DB_plugin_action_s *action, DB_playItem_t *it) {
    int var = deadbeef->conf_get_int ("playlist.stop_after_current", 0);
    var = 1 - var;
    deadbeef->conf_set_int ("playlist.stop_after_current", var);
    deadbeef->sendmessage (DB_EV_CONFIGCHANGED, 0, 0, 0);
    return 0;
}

static const char settings_dlg[] =
    "property \"Enable remote\" checkbox remote.enable 0;"
    "property \"Listen IP\" entry remote.listen \"\";\n"
    "property \"Port\" entry remote.port \"\";\n"
;


// define plugin interface
static DB_remote_plugin_t plugin = {
    .misc.plugin.api_vmajor = 1,
    .misc.plugin.api_vminor = 0,
    .misc.plugin.version_major = 0,
    .misc.plugin.version_minor = 1,
    .misc.plugin.type = DB_PLUGIN_MISC,
    .misc.plugin.id = "remote",
    .misc.plugin.name = "Remote control",
    .misc.plugin.descr = "Allows one to control player remotely over LAN",
    .misc.plugin.copyright =
        "BSD License\n"
        "Copyright (C) 2013 Henry Case <rectifier04@gmail.com>\n"
        "\n"
        "Redistribution and use in source and binary forms, with or without modification, are permitted\n"
        "provided that the following conditions are met:\n"
        "\t1. Redistributions of source code must retain the above copyright notice, this list of \n"
        "\tconditions and the following disclaimer.\n"
        "\t2. Redistributions in binary form must reproduce the above copyright notice, this list of \n"
        "\tconditions and the following disclaimer in the documentation and/or other materials\n"
        "\tprovided with the distribution.\n"
        "\n"
        "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n"
        "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"
        "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n"
        "IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,\n"
        "INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
        "LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n"
        "PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n"
        "WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n"
        "ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY\n"
        "OF SUCH DAMAGE.\n"
    ,
    .misc.plugin.website = "https://github.com/Aerol/deadbeef-remote",
    .misc.plugin.start = plugin_start,
    .misc.plugin.stop = plugin_stop,
    .misc.plugin.configdialog = settings_dlg,
};

DB_plugin_t *
remote_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}
