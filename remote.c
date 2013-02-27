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

#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

#include "remote.h"
#include <deadbeef/deadbeef.h>

#define trace(fmt,...)

static DB_remote_plugin_t plugin;
static DB_functions_t *deadbeef;
static int finished;
static intptr_t loop_tid;
static int need_reset = 0;

/*
  Listen for UDP packets for actions to perform.
  Next track, prev track, play/pause, stop, etc.
 */

static int plugin_start(void) {
    // Start plugin (duh)
    // Setup UDP listener to do stuff when receiving special datagrams
    // Dunno of what else to do. :/
    return 0;
}

static void plugin_stop(void) {
    // Stop listener and cleanup.
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

static DB_plugin_action_t action_play = {
    .title = "Play",
    .name = "play",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK (action_play_cb),
    .next = NULL
};

static DB_plugin_action_t action_stop = {
    .title = "Stop",
    .name = "stop",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_stop_cb),
    .next = &action_play
};

static DB_plugin_action_t action_prev = {
    .title = "Previous",
    .name = "prev",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_prev_cb),
    .next = &action_stop
};

static DB_plugin_action_t action_next = {
    .title = "Next",
    .name = "next",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_next_cb),
    .next = &action_prev
};

static DB_plugin_action_t action_toggle_pause = {
    .title = "Toggle Pause",
    .name = "toggle_pause",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_toggle_pause_cb),
    .next = &action_next
};

static DB_plugin_action_t action_play_pause = {
    .title = "Play\\/Pause",
    .name = "play_pause",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_play_pause_cb),
    .next = &action_toggle_pause
};

static DB_plugin_action_t action_play_random = {
    .title = "Play Random",
    .name = "playback_random",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_play_random_cb),
    .next = &action_play_pause
};

static DB_plugin_action_t action_seek_forward = {
    .title = "Seek Forward",
    .name = "seek_fwd",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_seek_forward_cb),
    .next = &action_play_random
};

static DB_plugin_action_t action_seek_backward = {
    .title = "Seek Backward",
    .name = "seek_back",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_seek_backward_cb),
    .next = &action_seek_forward
};

static DB_plugin_action_t action_volume_up = {
    .title = "Volume Up",
    .name = "volume_up",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_volume_up_cb),
    .next = &action_seek_backward
};

static DB_plugin_action_t action_volume_down = {
    .title = "Volume Down",
    .name = "volume_down",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_volume_down_cb),
    .next = &action_volume_up
};

static DB_plugin_action_t action_toggle_stop_after_current = {
    .title = "Toggle Stop After Current",
    .name = "toggle_stop_after_current",
    .flags = DB_ACTION_COMMON,
    .callback = DDB_ACTION_CALLBACK(action_toggle_stop_after_current_cb),
    .next = &action_volume_down
};

static DB_plugin_action_t *
hotkeys_get_actions (DB_playItem_t *it)
{
    return &action_toggle_stop_after_current;
}

// define plugin interface
static DB_remote_plugin_t plugin = {
    .misc.plugin.api_vmajor = 1,
    .misc.plugin.api_vminor = 0,
    .misc.plugin.version_major = 1,
    .misc.plugin.version_minor = 0,
    .misc.plugin.type = DB_PLUGIN_MISC,
    .misc.plugin.id = "remote",
    .misc.plugin.name = "Remote control",
    .misc.plugin.descr = "Allows one to control player remotely over LAN",
    .misc.plugin.copyright =
        "BSD License\n"
        "Copyright (C) 2013 Henry Case <rectifier04@gmail.com>\n"
        "\n"
        "Redistribution and use in source and binary forms, with or without modification, are permitted\n"
        "provided that the following conditions are met:"
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
    .misc.plugin.start = plugin_start,
    .misc.plugin.stop = plugin_stop,
};

DB_plugin_t *
remote_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}
