//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2016
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
// Note tthis file should only include calls to CATCIERGE_DEFINE_EVENT
// since it is meant to be included in multiple files.
//

#ifndef CATCIERGE_DEFINE_EVENT
#error "CATCIERGE_DEFINE_EVENT must be defined when " __file__ " is included"
#endif

CATCIERGE_DEFINE_EVENT(CATCIERGE_MATCH_GROUP_DONE, match_group_done,
	"Event when all steps for match group has been successfully performed. "
	"This is most likely what you want to trigger most stuff on.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_STATE_CHANGE, state_change,
	"State machine state changed.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_DO_LOCKOUT, do_lockout,
	"Triggered right before a lockout is performed.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_DO_UNLOCK, do_unlock,
	"Triggered right before a unlock is performed.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_SAVE_IMG, save_img,
	"Event after all images for a match group have been saved to disk.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_MATCH_DONE, match_done,
	"Triggered after each match in a match group.")

CATCIERGE_DEFINE_EVENT(CATCIERGE_FRAME_OBSTRUCTED, frame_obstructed,
	"Right after the camera view has been obstructed and the obstruct image "
	"has been saved.")

#ifdef WITH_RFID

CATCIERGE_DEFINE_EVENT(CATCIERGE_RFID_DETECT, rfid_detect,
	"Triggered on each time one of the readers have detected a tag")

CATCIERGE_DEFINE_EVENT(CATCIERGE_RFID_MATCH, rfid_match,
	"When an RFID match has been completed, this includes both readers "
	"having been read.")

#endif // WITH_RFID

// We undefine this so that it must be redefine each time the file is included.
#undef CATCIERGE_DEFINE_EVENT
