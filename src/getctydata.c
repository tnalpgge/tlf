/*
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2001-2002-2003-2004 Rein Couperus <pa0r@amsat.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

	/* ------------------------------------------------------------
	 *              Parse various  call  formats
	 *              Convert country data
	 *--------------------------------------------------------------*/


#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "dxcc.h"
#include "getpx.h"
#include "globalvars.h"		// Includes glib.h and tlf.h

/* check for calls which have no assigned country and no assigned zone,
 * e.g. airborn mobile /AM or maritime mobile /MM
 */
int location_unknown(const char *call) {

    return g_regex_match_simple("/AM$|/MM$", call,
	    (GRegexCompileFlags)0, (GRegexMatchFlags)0);
}

/* search for a full match of 'call' in the pfx table */
int find_full_match (const char *call) {
    int i, w;
    prefix_data *pfx;
    int pfxmax = prefix_count();

    w = -1;
    for (i = 0; i < pfxmax; i++) {
	pfx = prefix_by_index(i);
	if (strcmp(call, pfx->pfx) == 0) {
	    w = i;
	    break;
	}
    }
    return w;
}


/* search for the best mach of 'call' in pfx table */
int find_best_match (const char *call) {
    int bestlen = 0;
    int i, w;
    prefix_data *pfx;
    int pfxmax = prefix_count();

    w = -1;
    for (i = 0; i < pfxmax; i++) {
	int l;
	pfx = prefix_by_index(i);
	if (*pfx->pfx != call[0])
	    continue;

	l = strlen(pfx->pfx);
	if (l <= bestlen)
	    continue;

	if (strncmp(pfx->pfx, call, l) == 0) {
	    bestlen = l;
	    w = i;
	}
    }
    return w;
}

/* replace callsign area (K2ND/4 -> K4ND)
 *
 * for stations with multiple digits (LZ1000) it replaces the last digit
 * (may be wrong)
 */
void change_area(char *call, char area){
    int i;

    for (i = strlen(call) - 1; i > 0; i--) {
	if (isdigit(call[i])) {
	    call[i] = area;
	    break;
	}
    }
}


/* prepare and check callsign and look it up in dxcc data base
 *
 * returns index in data base or -1 if not found
 * if normalized_call ptr is not NULL retruns a copy of the normalized call
 * e.g. DL1XYZ/PA gives PA/DL1XYZ
 * caller has to free the copy after use
 */
int getpfxindex(char *checkcallptr, char **normalized_call)
{
    char checkcall[17] = "";
    char strippedcall[17] = "";


    int w = 0, abnormal_call = 0;
    size_t loc;

    g_strlcpy(strippedcall, checkcallptr, 17);

    if (strstr(strippedcall, "/QRP") ==
	    (strippedcall + strlen(strippedcall) - 4))
	/* drop QRP suffix */
	strippedcall[strlen(strippedcall) - 4] = '\0';

    /* go out if /MM, /AM or similar */
    if (location_unknown(strippedcall))
	strippedcall[0] = '\0';

    strncpy(checkcall, strippedcall, 16);

    loc = strcspn(checkcall, "/");

    if (loc != strlen(checkcall)) {		/* found a '/' */
	char checkbuffer[17] = "";
	char call1[17];
	char call2[17];

	strncpy(call1, checkcall, loc);		/* 1st part before '/' */
	call1[loc] = '\0';
	strcpy(call2, checkcall + loc + 1);	/* 2nd part after '/' */

	if (strlen(call2) < strlen(call1)
	    && strlen(call2) > 1) {
	    sprintf(checkcall, "%s/%s", call2, call1);
	    abnormal_call = 1;
	    loc = strcspn(checkcall, "/");
	}

	if (loc > 3) {

	    strncpy(checkbuffer, (checkcall + loc + 1),
		    (strlen(checkcall) + 1) - loc);

	    if (strlen(checkbuffer) == 1)
		checkcall[loc] = '\0';
	    loc = strcspn(checkcall, "/");
	}

	if (loc != strlen(checkcall)) {

	    if (loc < 5)
		checkcall[loc] = '\0';	/*  "PA/DJ0LN/P   */
	    else {		/*  DJ0LN/P       */
		strncpy(checkcall, checkcall, loc + 1);
	    }
	}

	/* ------------------------------------------------------------ */

	if ((strlen(checkbuffer) == 1) && isdigit(checkbuffer[0])) {	/*  /3 */

	    change_area (checkcall, checkbuffer[0]);

	} else if (strlen(checkbuffer) > 1)
	    strcpy(checkcall, checkbuffer);

    }

    /* -------------check full call exceptions first...--------------------- */

    if (abnormal_call == 1) {
	w = find_full_match(strippedcall);
    } else {
	w = find_best_match( strippedcall );
    }

    if (w < 0 && 0 != strcmp(strippedcall, checkcall)) {
	// only if not found in prefix full call exception list
	w = find_best_match( checkcall );
    }

    if (normalized_call != NULL)
	*normalized_call = strdup(checkcall);

    return w;
}

/* lookup dxcc cty number from callsign */
int getctynr(char *checkcall)
{
    int w;

    w = getpfxindex(checkcall, NULL);

    if (w >= 0)
	return prefix_by_index(w)->dxcc_index;
    else
	return 0;	/* no country found */
}


/* lookup various dxcc cty data from callsign
 *
 * side effect: set up various global variables
 */
int getctydata(char *checkcallptr)
{
    int w = 0, x = 0;
    char *normalized_call = NULL;

    w = getpfxindex(checkcallptr, &normalized_call);

    if (wpx == 1 || pfxmult == 1)
    	/* needed for wpx and other pfx contests */
	getpx(normalized_call);

    free (normalized_call);
    normalized_call = NULL;

    if (w >= 0 ) {
	x = prefix_by_index(w)->dxcc_index;
	sprintf(cqzone, "%02d", prefix_by_index(w) -> cq);
	sprintf(ituzone, "%02d", prefix_by_index(w) -> itu);
    }

    if (itumult != 1)
	strcpy(zone_export, cqzone);
    else
	strcpy(zone_export, ituzone);

    countrynr = x;
    g_strlcpy(continent, dxcc_by_index(countrynr) -> continent , 3);

    return (x);
}
