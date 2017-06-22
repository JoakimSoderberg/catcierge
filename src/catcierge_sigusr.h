//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2017
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
// Note tthis file should only include calls to CATCIERGE_SIGUSR_BEHAVIOR
// since it is meant to be included in multiple files.
//

#ifndef CATCIERGE_SIGUSR_BEHAVIOR
#error "CATCIERGE_SIGUSR_BEHAVIOR must be defined when " __file__ " is included"
#endif

CATCIERGE_SIGUSR_BEHAVIOR(SIGUSR_NONE, "none", "Nothing is peformed")
CATCIERGE_SIGUSR_BEHAVIOR(SIGUSR_LOCK, "lock", "Lock the cat door for lockout time")
CATCIERGE_SIGUSR_BEHAVIOR(SIGUSR_UNLOCK, "unlock", "Unlock the cat door")
CATCIERGE_SIGUSR_BEHAVIOR(SIGUSR_IGNORE, "ignore", "Ignores any events, until 'attention'")
CATCIERGE_SIGUSR_BEHAVIOR(SIGUSR_ATTENTION, "attention", "Stops ignoring events")

#undef CATCIERGE_SIGUSR_BEHAVIOR
