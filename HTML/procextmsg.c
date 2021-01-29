/* procextmsg.c -- Process External Messages */

/*---------------------------------------------------------------------------*
 |
 | PROJECT:
 |	Equipment Activity Controller (EAC)
 |	Jet Propulsion Laboratory, Pasadena, CA
 |
 |	A description of the EAC, its vision and characteristics is available
 |	from http://dsnra.jpl.nasa.gov/devel/eac.
 |
 | REVISION HISTORY:
 |
 |   Date            By               Description
 |
 | 15-Jun-96	 Thang Trinh	    Version 1.0 Delivery.
 | 30-Sep-98	 Thang Trinh	    Version 3.0 Delivery.
 | 08-Feb-99	 John Leflang	    Fixed RAC_RSP if_pwr test.
 |
 *--------------------------------------------------------------------------*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <Xm/Xm.h>
#include <Xm/Command.h>
#include <Xm/List.h>
#include <Xm/FileSB.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>

#include "rmd.h"
#include "net_eac.h"
#include "rdc_if.h"
#include "pcscode.h"
#include "xant.h"
#include "topform.h"
#include "cfg_rcv.h"
#include "offsets.h"
#include "scan.h"

#define	MAX_ALLOW_CMD	100
#define	MAX_RSP_LINES	16

extern Widget toplevel;
extern Widget ut_corr_text;
extern mutex_t sndq_lock;
extern int    snd_msqid;

void ClearBoreOffsets (Widget, XtPointer, XmPushButtonCallbackStruct *);
void ClearManOffsets (Widget, XtPointer, XmPushButtonCallbackStruct *);
void ClearRateOffsets (void);
void SetHPBW (void);
void process_scanreq (char *);
void proc_scanreq (char *, char *, char *, char *);
void ActivateBoresight ();
int ActivateXScan (Widget, int, XtPointer);

typedef struct {
    OFFSET_COORD coord;
    float	 pos;
} MOV_POS;

void calc_brsight_err (char *, float, float, POL);
int	ant_index (RMD_ENTRY *);
int	send_cmd (int, char *);
int	send_rsp (int, char *);
int     send_od (int, int, U16, char *);
char	*time_tag (void);
boolean is_onpoint (boolean, int);
void    LogPwr (int, char *, float, int, float, char *, float gain);
void    SetPol(char *, POL *);
void	lookup_source (char *);
void	precess_source (char *, float, float, char *);
void	load_source (char *, float, float, TRK_MODE);
void    process_minical_req (char *);
void    ConvRa (char *, char *);
void    ConvDec (char, char *, char *);
void    wait_completed (void);
void    set_if (int);
void    SetMirrorLabel (int);
int     EacChan (int);
int     RacChan (int);
double julian_date (char *);
void TrackNewSrc (Widget, PLOT_DATA *, XmPushButtonCallbackStruct *);

extern SR_POS	    sr_pos;
extern topform_p    topform;
extern cfg_rcv_p    cfg_rcv;
extern xscan_entry_p xscan_entry;

extern Widget	    toplevel;
extern Widget	    cmd_box;

extern int	    antno;
extern int	    linkno;
extern U16	    antpc;
extern int	    sgwfd;
extern int	    ctlfd;
extern int	    racfd;
extern int	    nsamples;
extern float	    rate1, rate2;
extern float	    gain[];
extern float	    onpoint_limit;
extern ANT_CNF	    antcnf;
extern OFFSET_DATA  m_offset;
extern PLOT_DATA    plot_data;
extern RMD_ENTRY    xrmd;
extern TRK_CNF	    trk_cnf;
extern RCV_CNF	    rcv_cnf;
extern SOURCE	    new_src;
extern SOURCE	    trk_src;
extern struct tm    *gmt;
extern struct resrc resrc;
extern boolean      hold_warning;
extern offsets_p    offsets;
extern SR_OFFSET    sr_offset[];
extern ROTATION_POS rot_pos;

extern int       num_xel_pts;
extern int       num_el_pts;
extern float     sum_xel_offsets;
extern float     sum_el_offsets;
extern int       num_xdec_pts;
extern int       num_dec_pts;
extern float     sum_xdec_offsets;
extern float     sum_dec_offsets;

extern OFFSET_DATA  m_offset;
extern OFFSET_DATA  b_offset ;
extern OFFSET_DATA  last_b_offset;
fivept_entry_p fivept_entry;

char	*allow[MAX_ALLOW_CMD] = {NULL};
char	*deny[]   =    {"TRACK",
			"AZPOS", "ELPOS", "AXPOS", "HAPOS", "DEPOS", "HXPOS",
			NULL};
FILE	*mini_log[MAX_CHAN] = {NULL};
FILE    *pwr_log[MAX_CHAN] = {NULL};
MOV_POS	move;
float tsys[MAX_CHAN] = {0.0};
CHAN_ASSIGNMENT eac_chan[MAX_CHAN];
CHAN_ASSIGNMENT rac_chan[MAX_CHAN];
time_t last_tsys_update[MAX_CHAN];
boolean logging_pwr[] = {false, false, false, false};

typedef struct apc_cmd_data {
    int  fea_index;
    char cmd[16];
    ANT_OP_STATE mode;
} APC_CMD_DATA;

void LogTemp (Widget w, int channel, XmToggleButtonCallbackStruct *state)
{
    if (state->set)
	logging_pwr[channel - 1] = true;
    else
	logging_pwr[channel - 1] = false;
}

void LogPwr (int eac_channel, char *timestr, float pwr, int rac_channel,
			float freq, char *pol, float gain)
{
    int  i;
    time_t timetag;
    char time_tag[15];

    if (!pwr_log[eac_channel]) {
        char chan_str[4];
        char num_str[4];
	/*
        char *name = {"init"};
	*/
        char pwr_header[] =
		{"DOY UTC Tsys RAC_chan Frequency Polarization Gain Az El\n"};
        FILE *OpenLog (String, String, String, String, String);
        int  num = 0;
        char fname[64];
        time_t    cur_time;
        struct tm *gmt;
        char year[5];
	char doy[4];
        chan_str[0] = eac_channel + 65;
        chan_str[1] = '\0';
	/*
        num_str[0] = '1';
        num_str[1] = '\0';
	*/

        cur_time = time ((time_t *) 0);
        gmt = gmtime (&cur_time);
        gmt->tm_isdst = -1;
        (void) strftime (doy, sizeof(doy), "%j", gmt);
        (void) strftime (year, sizeof(year), "%Y", gmt);

	/*
        while (num < 99 && name != NULL) {
	    num++;
            (void) sprintf (fname, "%s/t%s%s.%s%d:%s/storage/t%s%s.%s%d",
				resrc.log_dir, year, doy, chan_str, num,
				resrc.log_dir, year, doy, chan_str, num);
            name = XtFindFile (fname, NULL, 0, NULL);
        }

        XtFree (name);
	(void)sprintf (num_str, "%d", num);
	*/

	if ((pwr_log[eac_channel] = OpenLog
		(resrc.log_dir, "t", chan_str, "", pwr_header)) == NULL)
	    return;
	else {
            (void) sprintf (fname, "t%s%s.%s", year, doy, chan_str);
	    (void)fprintf (stderr, "%s opened\n", fname);
	}
    }

    timetag = (time_t)atoi (timestr);
    gmt = gmtime (&timetag);
    gmt->tm_isdst = -1;
    (void)strftime (time_tag, sizeof(time_tag), "%j %T", gmt);

    if ((i = ant_index (&xrmd)) == ERROR)
	return;

    (void)fprintf (pwr_log[eac_channel],
			"%s %.5G %d %.3f %s %.4G %.4f %.4f\n", time_tag,
			pwr * gain, rac_channel + 1, freq, pol, gain,
			xrmd.fea[i].angle.az, xrmd.fea[i].angle.el);
    (void)fflush (pwr_log[eac_channel]);
}

void process_ext_msg (XtPointer client_data, int *fdp, XtInputId *id)
{
    Widget parent = (Widget) client_data;
    int	   stat;
    char   buf[BUFSIZ];
    char   errstr[64];

    void process_fs_cmd (char *, int);
    void process_rac_rsp (char *);

    if ((stat = net_recv (*fdp, buf, sizeof buf, BLOCKING)) <= 0) {

	if (stat == NEOF) {
	    XtRemoveInput (*id);
	    (void) net_close (*fdp);

	    if (ctlfd == *fdp) {
		(void) sprintf (errstr, "Connection for EAC control\n"
					"was closed by foreign host.");
		ctlfd = ERROR;
	    }
	    else if (racfd == *fdp) {
		(void) sprintf (errstr, "Connection to RAC was\n"
					"closed by foreign host.");
		racfd = ERROR;
	    }
	    (void) warning (parent, errstr);
	}
	return;
    }

    if (linkno > 0) {
	switch (((MSG_HDR *) buf)->msg_id) {

	case FS_CMD:
	case RCTL_CMD: {
	    CMD_MSG *cmd_msg = (CMD_MSG *) buf;

	    cmd_msg->cmd[MAX_CMD_LEN - 1] = '\0';
	    process_fs_cmd (cmd_msg->cmd, cmd_msg->hdr.msg_id);
	    break;
	}
	case RAC_RSP: case RMON_RSP:	/* RMON_RSP is used temporarily only */
	case RAC_RSP|0x100: case RAC_RSP|0x200: case RAC_RSP|0x300: {
	    RAC_RSP_MSG *rsp_msg = (RAC_RSP_MSG *) buf;

	    buf[BUFSIZ - 1] = '\0';

	    if (cmd_box != NULL) {
		XmString  rsp[MAX_RSP_LINES];
		Widget	  list_w;
		char	  rsp_str[BUFSIZ];
		char	  *nl, *str = rsp_str;
		int	  i, count = 0;

		/* Don,t fill window with power meter readings */

		(void) strncpy (rsp_str, rsp_msg->rsp, 7);
		rsp_str[7] = '\0';
		if (strncmp (rsp_str, "if_pwr:", 7)) {
		    list_w = XmCommandGetChild (cmd_box, XmDIALOG_HISTORY_LIST);
		    (void) strncpy (rsp_str, rsp_msg->rsp, sizeof (rsp_str));

		    while ((nl = strchr (str, '\n')) != NULL &&
						count < MAX_RSP_LINES) {
		        *nl = '\0';
		        rsp[count++] = XmStringCreateLocalized (str);
		        str = nl + 1;
		    }

		    if (count < MAX_RSP_LINES)
		        rsp[count++] = XmStringCreateLocalized (str);

		    XmListAddItemsUnselected (list_w, rsp, count, 0);
		    XmListSetBottomPos (list_w, 0);
		    for (i = 0; i < count; i++)
		        XmStringFree (rsp[i]);
		}
	    }

	    process_rac_rsp (rsp_msg->rsp);
	    break;
	}
	default:
	    (void)fprintf (stderr, "xant: invalid message from pcfs or rac\n");
	    break;
	}
    }
}

/* ----------------------------------------------------------------------- */

int OffsetBwg (char *cmdstr, OFFSET_TYPE offset_type)
{
	int     status;
        float   offst1, offst2;
        char    offst_val[16];
        char    offst_coord[80];
        char    *errstr = "poffset: rejected. Invalid command.";
        char  odstr[MAX_OD_LEN];
        boolean reset   = true;

        if (sscanf (cmdstr, "%*s %s %f %f", offst_coord, &offst1,
                                                         &offst2) == 3) {
            if (!strcasecmp (offst_coord, "AZEL")) {
                m_offset.az_pos_offset = offst1;
                m_offset.el_pos_offset = offst2;

                (void)sprintf (odstr, "PO AZ=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
                (void)sprintf (odstr, "PO EL=%.1f", offst2 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 2;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XELEL")) {
                m_offset.xel_pos_offset = offst1;
                m_offset.el_pos_offset  = offst2;

                (void)sprintf (odstr, "PO XEL=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
                (void)sprintf (odstr, "PO EL=%.1f", offst2 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 2;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "HADEC")) {
                m_offset.ha_pos_offset  = offst1;
                m_offset.dec_pos_offset = offst2;

                (void)sprintf (odstr, "PO HA=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
                (void)sprintf (odstr, "PO DEC=%.1f", offst2 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 2;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XDECDEC")) {
                m_offset.xdec_pos_offset  = offst1;
                m_offset.dec_pos_offset   = offst2;

                (void)sprintf (odstr, "PO XDEC=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
                (void)sprintf (odstr, "PO DEC=%.1f", offst2 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 2;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else {
		status = -1;
	    }

	    return (status);
        }
        else if (sscanf (cmdstr, "%*s %s %f", offst_coord, &offst1) == 2) {
            if (!strcasecmp (offst_coord, "AZ")) {
		if (offset_type == BORESIGHT)
                    b_offset.az_pos_offset = offst1;
		else
                    m_offset.az_pos_offset = offst1;

                (void)sprintf (odstr, "PO AZ=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "EL")) {
		if (offset_type == BORESIGHT)
                    b_offset.el_pos_offset = offst1;
		else
                    m_offset.el_pos_offset = offst1;

                (void)sprintf (odstr, "PO EL=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XEL")) {
		if (offset_type == BORESIGHT)
                    b_offset.xel_pos_offset = offst1;
		else
                    m_offset.xel_pos_offset = offst1;

                (void)sprintf (odstr, "PO XEL=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "HA")) {
		if (offset_type == BORESIGHT)
                    b_offset.ha_pos_offset  = offst1;
		else
                    m_offset.ha_pos_offset  = offst1;

                (void)sprintf (odstr, "PO HA=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "DEC")) {
		if (offset_type == BORESIGHT)
                    b_offset.dec_pos_offset  = offst1;
		else
                    m_offset.dec_pos_offset  = offst1;

                (void)sprintf (odstr, "PO DEC=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XDEC")) {
		if (offset_type == BORESIGHT)
                    b_offset.xdec_pos_offset  = offst1;
		else
                    m_offset.xdec_pos_offset  = offst1;

                (void)sprintf (odstr, "PO XDEC=%.1f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "Y")) {
                (void)sprintf (odstr, "PO Y=%.0f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		sr_offset[rot_pos].y = offst1;
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	    }
            else if (!strcasecmp (offst_coord, "Z")) {
                (void)sprintf (odstr, "PO Z=%.0f", offst1 * 1000.0);
                (void)send_od (sgwfd, linkno, antpc, odstr);
		sr_offset[rot_pos].z = offst1;
		status = 1;

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	    }
            else {
		status = -1;
            }

            return (status);
        }
        else
            return (-1);
}

/* ----------------------------------------------------------------------- */

void process_fs_cmd (char *cmdstr, int msg_id)
{
    char  cmd[MAX_CMD_LEN];
    char  rsp[MAX_RSP_LEN];
    char  odstr[MAX_OD_LEN];
    boolean reset = true;

    (void) sscanf (cmdstr, "%s", cmd);

    if (!strcasecmp (cmd, "CALON") || !strcasecmp (cmd, "CALOFF") ||
	!strcasecmp (cmd, "LOAD")  || !strcasecmp (cmd, "SKY")) {

	if (racfd != -1)
	    (void) send_cmd (racfd, cmdstr);
	else {
	    (void) sprintf (rsp, "%s: rejected. RAC not connected.", cmd);
	    (void) send_rsp (ctlfd, rsp);
	}
    }
    else if (!strcasecmp (cmd, "LOAD_MODEL")) {
	char model_name[64];
	char  odstr[MAX_OD_LEN];

	int num_args;

	num_args = sscanf (cmdstr, "%*s %63s", model_name);
	if (num_args == 1) {
	    (void)sprintf (odstr, "LOAD %s.sem", model_name);
	    (void)send_od (sgwfd, linkno, antpc, odstr);
	    (void) send_rsp (ctlfd, "load_model: completed");
	}
	else
	    (void) send_rsp (ctlfd, "load_model: rejected. Bad arglist.");
    }
    else if (!strcasecmp (cmd, "LO")) {
	if (racfd != -1) {
	    int num_args;
	    float freq, tone_space, tone_off;
	    char name[16], sideband[16], pol[4];

	    num_args = sscanf (cmdstr, "%*s %15s %f %15s %3s %f %f",
			name, &freq, sideband, pol, &tone_space, &tone_off);
	    if (num_args == 6) {
	        (void) send_cmd (racfd, cmdstr);
	    }
	    else
	        (void) send_rsp (ctlfd, "lo: rejected. Bad arglist.");
	}
	else {
	    (void) sprintf (rsp, "%s: rejected. RAC not connected.", cmd);
	    (void) send_rsp (ctlfd, rsp);
	}
    }
    else if (!strcasecmp (cmd, "AZEL?")) {
	int i;

	if ((i = ant_index (&xrmd)) == ERROR)
	    return;

	(void) sprintf (rsp, "azel: %8.3f, %8.3f", xrmd.fea[i].angle.az,
						   xrmd.fea[i].angle.el);
	(void) send_rsp (ctlfd, rsp);
    }
    else if (!strcasecmp (cmd, "MOVE")) {
        float   pos;
        char    axis[4];

        void check_on_pos ();

        if (sscanf (cmdstr, "%*s %s %f", axis, &pos) == 2) {

            axis[3] = '\0';

            if (!strcasecmp (axis, "AZ")) {
                (void) send_od (sgwfd, linkno, APA_PC, "ASC STOP");
                (void) sprintf (odstr, "ASC MOVE AZ %08.4f", pos);
                (void) send_od (sgwfd, linkno, APA_PC, odstr);
                (void) fprintf (stderr, "%s\n", odstr);
                (void) send_rsp (ctlfd, "move: processing");
                move.coord = AZ;
                move.pos   = pos;

                XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel),
					5000, check_on_pos, &move);
            }
            else if (!strcasecmp (axis, "EL")) {
                (void) send_od (sgwfd, linkno, APA_PC, "ASC STOP");
                (void) sprintf (odstr, "ASC MOVE EL %08.4f", pos);
                (void) send_od (sgwfd, linkno, APA_PC, odstr);
                (void) fprintf (stderr, "%s\n", odstr);
                (void) send_rsp (ctlfd, "move: processing");
                move.coord = EL;
                move.pos   = pos;

                XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel),
					5000, check_on_pos, &move);
            }
            else if (!strcasecmp (axis, "X") ||
                     !strcasecmp (axis, "Y") || !strcasecmp (axis, "Z")) {
                if (pos >= -5.0 && pos <= 5.0) {
                    (void) sprintf (odstr, "SRC MOVE %s = %8.2f", axis, pos);
                   (void) send_od (sgwfd, linkno, APA_PC, odstr);
                    (void) fprintf (stderr, "%s\n", odstr);
                    (void) send_rsp (ctlfd, "move: completed");
                }
                else
                    (void) send_rsp (ctlfd,
                                     "move: rejected. Position out of range.");
            }
            else if (!strcasecmp (axis, "R")) {
                if (pos >= 0.0 && pos <= 360.0) {
                    (void) sprintf (odstr, "SRC MOVE R %8.2f", pos);
                    (void) send_od (sgwfd, linkno, APA_PC, odstr);
                    (void) fprintf (stderr, "%s\n", odstr);
                    (void) send_rsp (ctlfd, "move: completed");
                }
                else
                    (void) send_rsp (ctlfd,
                                     "move: rejected. Rotation out of range.");
            }
            else
                (void) send_rsp (ctlfd, "move: rejected. Invalid axis.");
        }
        else
            (void) send_rsp (ctlfd, "move: rejected. Invalid command.");
    }
    else if (!strcasecmp (cmd, "AZEL")) {
	int	num_args;
	float	az, el;

	num_args = sscanf (cmdstr, "%*s %f %f", &az, &el);
	if (num_args == 2){
	    load_source ("AzEl", az, el, MAINT);
	    (void) send_rsp (ctlfd, "azel: completed");

	    (void)is_onpoint (reset, SOURCE_CHANGE_WAIT);
	}
	else
	    (void) send_rsp (ctlfd, "azel: rejected. Wrong no. of args.");
    }
    else if (!strcasecmp (cmd, "OFFSET")) {
        int i, status;
        float   offst1, offst2;
        char    offst_coord[80];
        char    *errstr = "offset: rejected. Invalid command.";
        boolean reset   = true;

	if ((i = ant_index (&xrmd)) == ERROR)
	    return;

        if (xrmd.fea[i].ant_type == ANT_BWG) {
            int status = OffsetBwg (cmdstr, MANUAL);

            if (status == 1)
                (void)send_rsp (ctlfd, "poffset: completed");
            else if (status == 2)
                (void)send_rsp (ctlfd, "poffsets: completed");
            else
                (void)send_rsp (ctlfd, errstr);

            return;
        }

        if (sscanf (cmdstr, "%*s %s %f %f", offst_coord, &offst1,
                                                         &offst2) == 3)
            if (!strcasecmp (offst_coord, "AZEL")) {
                m_offset.az_pos_offset = offst1;
                m_offset.el_pos_offset = offst2;

                (void)sprintf (odstr, "ACS AZPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)sprintf (odstr, "ACS ELPOS %f", offst2);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XELEL")) {
                m_offset.xel_pos_offset = offst1;
                m_offset.el_pos_offset  = offst2;

                (void)sprintf (odstr, "ACS AXPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)sprintf (odstr, "ACS ELPOS %f", offst2);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "HADEC")) {
                m_offset.ha_pos_offset  = offst1;
                m_offset.dec_pos_offset = offst2;

                (void)sprintf (odstr, "ACS HAPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)sprintf (odstr, "ACS DEPOS %f", offst2);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XDECDEC")) {
                m_offset.xdec_pos_offset  = offst1;
                m_offset.dec_pos_offset   = offst2;

                (void)sprintf (odstr, "ACS HXPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)sprintf (odstr, "ACS DEPOS %f", offst2);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else
                (void) send_rsp (ctlfd, errstr);

        else if (sscanf (cmdstr, "%*s %s %f", offst_coord, &offst1) == 2)
            if (!strcasecmp (offst_coord, "AZ")) {
                m_offset.az_pos_offset = offst1;

                (void)sprintf (odstr, "ACS AZPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "EL")) {
                m_offset.el_pos_offset = offst1;

                (void)sprintf (odstr, "ACS ELPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XEL")) {
                m_offset.xel_pos_offset = offst1;

                (void)sprintf (odstr, "ACS AXPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "HA")) {
                m_offset.ha_pos_offset  = offst1;

                (void)sprintf (odstr, "ACS HAPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "DEC")) {
                m_offset.dec_pos_offset  = offst1;

                (void)sprintf (odstr, "ACS DEPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else if (!strcasecmp (offst_coord, "XDEC")) {
                m_offset.xdec_pos_offset  = offst1;

                (void)sprintf (odstr, "ACS HXPOS %f", offst1);
                (void)send_od (sgwfd, linkno, APA_PC, odstr);
                (void)send_od (sgwfd, linkno, APA_PC, "ACS REQOFF");
                (void)send_rsp (ctlfd, "offset: completed");

                (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
            }
            else
                (void) send_rsp (ctlfd, errstr);
        else
            (void) send_rsp (ctlfd, errstr);
    }
    else if (!strcasecmp (cmd, "POFFSET") || !strcasecmp (cmd, "POFFSETS")) {
        int status = OffsetBwg (cmdstr, MANUAL);

        if (status == 1)
            (void)send_rsp (ctlfd, "poffset: completed");
        else if (status == 2)
            (void)send_rsp (ctlfd, "poffsets: completed");
        else
            (void)send_rsp (ctlfd, "poffset: rejected. Invalid command.");
    }
    else if (!strcasecmp (cmd, "OFFSET?")) {
	(void) sprintf (rsp, "offset: %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f",
			m_offset.az_pos_offset,  m_offset.el_pos_offset,
			m_offset.xel_pos_offset, m_offset.ha_pos_offset,
			m_offset.dec_pos_offset, m_offset.xdec_pos_offset);
	(void) send_rsp (ctlfd, rsp);
    }
    else if (!strcasecmp (cmd, "ROFFSET")) {
	Widget coord_widget;
        char    roffst_str[16];
	char    *ptr;
	char    roffst_coord[8];
        OFFSET_COORD offst_coord;

        roffst_coord[7] = '\0';
        if (sscanf (cmdstr, "%*s %5s %15s", roffst_coord, &roffst_str) == 2) {
	    if (!strcasecmp (roffst_coord, "AZ")) {
		offst_coord = AZ;
	    }
	    else if (!strcasecmp (roffst_coord, "EL")) {
		offst_coord = EL;
	    }
	    else if (!strcasecmp (roffst_coord, "XEL")) {
		offst_coord = XEL;
	    }
	    else if (!strcasecmp (roffst_coord, "HA")) {
		offst_coord = HA;
	    }
	    else if (!strcasecmp (roffst_coord, "DEC")) {
		offst_coord = DEC;
	    }
	    else if (!strcasecmp (roffst_coord, "XDEC")) {
		offst_coord = XDEC;
	    }
	    else {
                (void) send_rsp (ctlfd,
                        "roffset: rejected. Coord not valid.");
		return;
	    }

	    /*
	    ptr = roffst_coord;
	    while ((*ptr++ = toupper (*ptr)) != '\0');
            (void)sprintf (odstr, "RO %s=%.1f", roffst_coord, roffst);
            (void)send_od (sgwfd, linkno, antpc, odstr);
            */	

	    XtVaSetValues (offsets->offset_text,
				XmNuserData, offst_coord, NULL);
	    XtVaSetValues (offsets->offsets,
				XmNtitle, "Rate Offsets (mdeg/sec)", NULL);
	    XmTextFieldSetString (offsets->offset_text, roffst_str);
	    ApplyOffset (NULL, NULL, NULL);
            (void)send_rsp (ctlfd, "roffset: completed");
        }
        else {
            (void) send_rsp (ctlfd,
                        "roffset: rejected. Wrong number of args.");
        }
    }
    else if (!strcasecmp (cmd, "SOURCE")) {
	int n;
	float ra, dec, pra, pdec;
	char  srcname[NAME_LEN];
	char  epoch[16];

	n = sscanf (cmdstr, "%*s %s %f %f %15s %f %f", srcname,
				&ra, &dec, epoch, &pra, &pdec);

	if (n == 1) {
	    lookup_source (srcname);
	    (void) send_rsp (ctlfd, "source: completed");
	}
	else if (n == 3) {
	    load_source (srcname, ra, dec, SIDEREAL);
	    (void) send_rsp (ctlfd, "source: completed");
	}
	else if (n == 4) {
            if (!strcmp (epoch, "J2000") || !strcmp (epoch, "B1950")) {
	        precess_source (srcname, ra, dec, epoch);
	        (void) send_rsp (ctlfd, "source: completed");
	    }
	    else
	        (void) send_rsp (ctlfd, "source: rejected. Invalid epoch.");
	}
	else if (n == 6) {
	    load_source (srcname, pra, pdec, SIDEREAL);
	    (void) send_rsp (ctlfd, "source: completed");
	}
	else
	    (void) send_rsp (ctlfd, "source: rejected. Invalid command.");
    }
    else if (!strcasecmp (cmd, "ONPOINT?")) {
	if (is_onpoint (!reset, 0))
	    (void) send_rsp (ctlfd, "onpoint: Tracking");
	else
	    (void) send_rsp (ctlfd, "not onpoint: Slewing");
    }
    else if (!strcasecmp (cmd, "ONSOURCE") || !strcasecmp (cmd, "ONSOURCE?")) {
	void check_on_src ();

	check_on_src (msg_id);
    }
    else if (!strcasecmp (cmd, "RADLOG")) {
	int   rac_chan;
	char  chan;
	char  state[8];

	if (racfd > 0) {
	    if (sscanf (cmdstr, "%*s %3s %c", state, &chan) == 1) {
		if (!strcasecmp (state, "ON")) {
		    (void) send_cmd (racfd, "Rad_Disk_On");
	            (void) send_rsp (ctlfd, "radlog: completed");
		}
		else if (!strcasecmp (state, "OFF")) {
		    (void) send_cmd (racfd, "Rad_Disk_Off");
	            (void) send_rsp (ctlfd, "radlog: completed");
		}
		else
	            (void) send_rsp (ctlfd,
		            "radlog: rejected. Use on or off");
	    }
	    else if (sscanf (cmdstr, "%*s %s %d", state, &chan) == 2) {
		if (eac_chan[toupper(chan) - 65].freq > 0.0) {
		    if ((rac_chan = RacChan (toupper(chan) - 65)) > -1) {
		        if (!strcasecmp (state, "ON")) {
		            switch (rac_chan) {
		                case 1:
			            (void) send_cmd (racfd, "Rad_Disk_On 1");
	                            (void) send_rsp (ctlfd,
						"radlog: completed");
			            break;
		                case 2:
			            (void) send_cmd (racfd, "Rad_Disk_On 2");
	                            (void) send_rsp (ctlfd,
						"radlog: completed");
			            break;
		                default:
	                            (void) send_rsp (ctlfd,
		                    "radlog: rejected. Check channel \
				    assignment.");
		            }
			}
		        else if (!strcasecmp (state, "OFF")) {
		            switch (chan) {
		                case 1:
			            (void) send_cmd (racfd, "Rad_Disk_Off 1");
	                            (void) send_rsp (ctlfd,
						"radlog: completed");
			            break;
		                case 2:
			            (void) send_cmd (racfd, "Rad_Disk_Off 2");
	                            (void) send_rsp (ctlfd,
						"radlog: completed");
			            break;
		                default:
	                            (void) send_rsp (ctlfd,
		                    "radlog: rejected. Check channel \
				    assignment.");
		            }
		        }
		        else
	                    (void) send_rsp (ctlfd,
		                    "radlog: rejected. Use on or off");
		    }
		    else
	                (void) send_rsp (ctlfd,
		            "radlog: rejected. Check channel assignment.");
		}
		else
		    (void) send_rsp (ctlfd,
				"radlog: rejected. EAC channel unassigned.");
	    }
	    else
	        (void) send_rsp (ctlfd,
				"radlog: rejected. Bad arg count.");
	}
	else
	    (void) send_rsp (ctlfd,
		            "radlog: rejected. EAC not connected to RAC");
    }
    else if (!strcasecmp (cmd, "WAIT_FOR_FEED")) {
        char *name = "wait_for_feed";
	THR_PTRS thr;

	thr.cmd = name;
	thr.arg = NULL;
        (void) thr_start (&thr);
	free (name);
    }
    else if (!strcasecmp (cmd, "WAIT_FOR_ON_POINT")) {
	char rsp[MAX_RSP_LEN];
	float new_limit;
	int args;

	if (sscanf (cmdstr, "%*s %f", &new_limit) == -1) {
	    (void) sprintf (rsp, "limit: completed. %.4f", onpoint_limit);
	    (void) send_rsp (ctlfd, rsp);
	}
	else if (sscanf (cmdstr, "%*s %f", &new_limit) == 1) {
	    char factor[64];
	    
	    if (new_limit >= 0.001 && new_limit <= 1.0) {
		(void)sprintf (factor, "%f", new_limit);
		XmTextFieldSetString (topform->onpoint_lim, factor);
		SetOnPointLimit (NULL, NULL, 1);
	        (void) sprintf (rsp, "limit: completed. %.4f", onpoint_limit);
	        (void) send_rsp (ctlfd, rsp);
	    }
	    else
	        (void) send_rsp (ctlfd, "limit: rejected. Out of range.");
	}
	else
	    (void) send_rsp (ctlfd, "limit: rejected. Bad arg count.");
    }
    else if (!strcasecmp (cmd, "WEATHER") || !strcasecmp (cmd, "WEATHER?")) {
	(void) sprintf (rsp, "weather: %.1fC, %.1fmB, %.0f%%",
			xrmd.wthr.temp, xrmd.wthr.press, xrmd.wthr.hum);
	(void) send_rsp (ctlfd, rsp);
    }
    else if (!strncasecmp (cmd, "ANTENNA=", 8)) {
	void process_anteq ();

	process_anteq (cmdstr);
    }
    else if (!strncasecmp (cmd, "ASSIGN", 6)) {
	void set_channel ();

	set_channel (cmdstr);
    }
    else if (!strncasecmp (cmd, "BEEP", 4)) {
	fprintf (stderr, "\a\a\a\a\a\a\a\n");
	(void) send_rsp (ctlfd, "beep: completed");
    }
    else if (!strncasecmp (cmd, "CLR", 3)) {
	char odstr[MAX_OD_LEN];
	char type[16];
	char coord[16];
	char rsp[32];
	OFFSET_COORD offst_coord;

        if (sgwfd == -1) {
	    (void) send_rsp (ctlfd, "clr: rejected. Gateway not connected");
	    return;
	}

	if (sscanf (cmdstr, "%*s %15s %15s", type, coord) == 2) {
	    if (!strcasecmp (type, "po")) {
		if (!strcasecmp (coord, "all")) {
	            ClearManOffsets (NULL, NULL, NULL);
	            (void) send_rsp (ctlfd, "clr po all: completed");
		    return;
		}
		else if (!strcasecmp (coord, "az")) {
		    offst_coord = AZ;
		}
		else if (!strcasecmp (coord, "el")) {
		    offst_coord = EL;
		}
		else if (!strcasecmp (coord, "xel")) {
		    offst_coord = XEL;
		}
		else if (!strcasecmp (coord, "ha")) {
		    offst_coord = HA;
		}
		else if (!strcasecmp (coord, "dec")) {
		    offst_coord = DEC;
		}
		else if (!strcasecmp (coord, "xdec")) {
		    offst_coord = XDEC;
		}
		else {
	            (void) send_rsp (ctlfd,
				"clr: rejected. Unknown coord");
		    return;
		}
		XtVaSetValues (offsets->offset_text,
				 XmNuserData, offst_coord, NULL);
	        XtVaSetValues (offsets->offsets,
				XmNtitle, "Position Offsets (deg)", NULL);
		XmTextFieldSetString (offsets->offset_text, "0.0");
		ApplyOffset (NULL, NULL, NULL);
	        (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
		(void)sprintf (rsp, "clr po %s: completed", coord);
	        (void) send_rsp (ctlfd, rsp);
	    }
	    else if (!strcasecmp (type, "ro")) {
		if (!strcasecmp (coord, "all")) {
	            ClearRateOffsets ();
	            (void) send_rsp (ctlfd, "clr ro all: completed");
		    return;
		}
		else if (!strcasecmp (coord, "az")) {
		    offst_coord = AZ;
		}
		else if (!strcasecmp (coord, "el")) {
		    offst_coord = EL;
		}
		else if (!strcasecmp (coord, "xel")) {
		    offst_coord = XEL;
		}
		else if (!strcasecmp (coord, "ha")) {
		    offst_coord = HA;
		}
		else if (!strcasecmp (coord, "dec")) {
		    offst_coord = DEC;
		}
		else if (!strcasecmp (coord, "xdec")) {
		    offst_coord = XDEC;
		}
		else {
	            (void) send_rsp (ctlfd,
				"clr: rejected. Unknown coord");
		    return;
		}

		XtVaSetValues (offsets->offset_text,
				 XmNuserData, offst_coord, NULL);
	        XtVaSetValues (offsets->offsets,
				XmNtitle, "Rate Offsets (mdeg/sec)", NULL);
		XmTextFieldSetString (offsets->offset_text, "0.0");
		ApplyOffset (NULL, NULL, NULL);
	        (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
		(void)sprintf (rsp, "clr ro %s: completed", coord);
	        (void) send_rsp (ctlfd, rsp);
	    }
	    else if (!strcasecmp (type, "bso")) {
		if (!strcasecmp (coord, "all")) {
	            ClearBoreOffsets (NULL, NULL, NULL);
	            (void) send_rsp (ctlfd, "clr bso all: completed");
		}
		else if (!strcasecmp (coord, "el")) {
		    num_el_pts  = 0;
		    sum_el_offsets  = 0.0;
		    b_offset.el_pos_offset   = 0.0;
		    last_b_offset.el_pos_offset   = 0.0;
                    (void)sprintf (odstr, "PO EL=%.4f",
					m_offset.el_pos_offset * 1000.0);
                    (void)send_od (sgwfd, linkno, antpc, odstr);
                    (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	            (void) send_rsp (ctlfd, "clr bso el: completed");
		}
		else if (!strcasecmp (coord, "xel")) {
		    num_xel_pts = 0;
		    sum_xel_offsets = 0.0;
		    b_offset.xel_pos_offset  = 0.0;
		    last_b_offset.xel_pos_offset  = 0.0;
                    (void)sprintf (odstr, "PO XEL=%.4f",
					m_offset.xel_pos_offset * 1000.0);
                    (void)send_od (sgwfd, linkno, antpc, odstr);
                    (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	            (void) send_rsp (ctlfd, "clr bso xel: completed");
		}
		else if (!strcasecmp (coord, "dec")) {
		    num_dec_pts  = 0;
		    sum_dec_offsets  = 0.0;
		    b_offset.dec_pos_offset  = 0.0;
		    last_b_offset.dec_pos_offset  = 0.0;
                    (void)sprintf (odstr, "PO DEC=%.4f",
					m_offset.dec_pos_offset * 1000.0);
                    (void)send_od (sgwfd, linkno, antpc, odstr);
                    (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	            (void) send_rsp (ctlfd, "clr bso dec: completed");
		}
		else if (!strcasecmp (coord, "xdec")) {
		    num_xdec_pts = 0;
		    sum_xdec_offsets = 0.0;
		    b_offset.xdec_pos_offset = 0.0;
		    last_b_offset.xdec_pos_offset = 0.0;
                    (void)sprintf (odstr, "PO XDEC=%.4f",
					m_offset.xdec_pos_offset * 1000.0);
                    (void)send_od (sgwfd, linkno, antpc, odstr);
                    (void)is_onpoint (reset, OFFSET_CHANGE_WAIT);
	            (void) send_rsp (ctlfd, "clr bso xdec: completed");
		}
		else {
	            (void) send_rsp (ctlfd,
				"clr: rejected. Unknown coord");
		    return;
		}
	    }
	    else
	        (void) send_rsp (ctlfd, "clr: rejected. Unknown type");
	}
	else
	    (void) send_rsp (ctlfd, "clr: rejected. Wrong no. args");
    }
    else if (!strncasecmp (cmd, "DELTA_UT", 8)) {
	int ms;
	char offset[4];

	if (sscanf (cmdstr, "%*s %3d", &ms) == 1) {
	    (void)sprintf (offset, "%d", ms);
	    XmTextFieldSetString (ut_corr_text, offset);
	    if (SaveTrkCnf (toplevel, NULL, NULL)) {
	        (void) sprintf (rsp, "delta_ut: completed");
	        (void) send_rsp (ctlfd, rsp);
	    }
	    else
	        (void) send_rsp (ctlfd, "delta_ut: rejected. Out of range.");
	}
    }
    else if (!strncasecmp (cmd, "FEEDPOS", 7)) {
	FEED_POS pos;
	char posstr[4];
	char label[16];
        XmString xms;

	if (sscanf (cmdstr, "%*s %3s", posstr) == 1) {
		if (racfd > 0) {
		    /*
		    (void)sprintf (odstr, "mirror %1d", pos);
		    (void) send_cmd (racfd, odstr);
		    */
		    if (!strcmp (posstr, "P1"))
			pos = P1;
		    else if (!strcmp (posstr, "P2"))
			pos = P2;
		    else if (!strcmp (posstr, "P3"))
			pos = P3;
		    else if (!strcmp (posstr, "P4"))
			pos = P4;
		    else if (!strcmp (posstr, "P5"))
			pos = P5;
		    else if (!strcmp (posstr, "P6"))
			pos = P6;
		    else if (!strcmp (posstr, "P1L"))
			pos = P1L;
		    else if (!strcmp (posstr, "P1R"))
			pos = P1R;
		    else {
			(void)fprintf (stderr, "Invalid feed pos\n");
			pos = P;
		    }

                    rcv_cnf.feed_pos = pos;
		    SetMirrorLabel (rcv_cnf.feed_pos);
		    ConfigureRcv (toplevel, NULL, NULL);
		}
		else
	            (void) send_rsp (ctlfd,
		            "feedpos: rejected. EAC not connected to RAC");
	}
	else
	    (void) send_rsp (ctlfd, "feedpos: rejected. Arg incorrect.");
    }
    else if (!strncasecmp (cmd, "TLOG", 4)) {
	char chan_id;
	char state[4];
	int  n, chan, i;

	if (sscanf (cmdstr, "%*s %c %3s", &chan_id, state) == 2) {
	    n = toupper(chan_id) - 65;
	    if (n >= 0 && n < MAX_CHAN) {
	        if (!strcasecmp (state, "on")) {
	            logging_pwr[n] = true;
	            (void) send_rsp (ctlfd, "tlog: completed");
	        }
	        else if (!strcasecmp (state, "off")) {
	            logging_pwr[n] = false;
	            (void) send_rsp (ctlfd, "tlog: completed");
	        }
	        else {
	            (void) send_rsp (ctlfd, "tlog: rejected. Bad state.");
	        }
	    }
	    else
	        (void) send_rsp (ctlfd, "tlog: rejected. Bad chan id.");
	}
	else
	    (void) send_rsp (ctlfd, "tlog: rejected. Args bad.");
    }
    else if (!strncasecmp (cmd, "XSCAN", 5)) {
	void process_xscanreq ();

	process_xscanreq (cmdstr);
    }
    else if (!strncasecmp (cmd, "PSCAN", 5)) {
	void process_pscanreq ();

	process_pscanreq (cmdstr);
    }
    else if (!strncasecmp (cmd, "SCAN", 4)) {
	void process_scanreq ();

	process_scanreq (cmdstr);
    }
    else if (!strncasecmp (cmd, "SET_RAD_INT", 11)) {
	int value;

	if (sscanf (cmdstr, "%*s %d", &value) == 1) {
	    if (value >= 0 && value <= 9) {
	        (void)sprintf (odstr, "SET_RAD_INT %d", value);
	        (void) send_cmd (racfd, odstr);
	        (void) send_rsp (ctlfd, "set_rad_int: completed");
	    }
	    else
	        (void) send_rsp (ctlfd, "set_rad_int: rejected. Bad value.");
	}
	else
	    (void) send_rsp (ctlfd, "set_rad_int: rejected. Arg incorrect.");
    }
    else if (!strncasecmp (cmd, "REFCHAN", 7)) {
	int n;
	char ref, secondary;

	n = sscanf (cmdstr, "%*s %c %c", &ref, &secondary);
	if (n == 1 || n == 2) {
	    switch (toupper(ref)) {
		case 'A':
	            rcv_cnf.scan_ref_index = 0;
		    XmToggleButtonGadgetSetState
				(cfg_rcv->scan_ref_a, True, True);
		    break;
		case 'B':
	            rcv_cnf.scan_ref_index = 1;
		    XmToggleButtonGadgetSetState
				(cfg_rcv->scan_ref_b, True, True);
		    break;
		case 'C':
	            rcv_cnf.scan_ref_index = 2;
		    XmToggleButtonGadgetSetState
				(cfg_rcv->scan_ref_c, True, True);
		    break;
		case 'D':
	            rcv_cnf.scan_ref_index = 3;
		    XmToggleButtonGadgetSetState
				(cfg_rcv->scan_ref_d, True, True);
		    break;
		default:
	            (void) sprintf (rsp,
				"%s: rejected. First arg out of range", cmd);
	            (void) send_rsp (ctlfd, rsp);
		    return;
	    }

	    if (n == 2) {
	        switch (toupper(secondary)) {
		    case 'A':
	                rcv_cnf.scan_secondary_index = 0;
		        XmToggleButtonGadgetSetState
					(cfg_rcv->scan_ref2_a, True, True);
		        break;
		    case 'B':
	                rcv_cnf.scan_secondary_index = 1;
		        XmToggleButtonGadgetSetState
					(cfg_rcv->scan_ref2_b, True, True);
		        break;
		    case 'C':
	                rcv_cnf.scan_secondary_index = 2;
		        XmToggleButtonGadgetSetState
					(cfg_rcv->scan_ref2_c, True, True);
		        break;
		    case 'D':
	                rcv_cnf.scan_secondary_index = 3;
		        XmToggleButtonGadgetSetState
					(cfg_rcv->scan_ref2_d, True, True);
		        break;
		    default:
	                (void) sprintf (rsp,
				"%s: rejected. Second arg out of range", cmd);
	                (void) send_rsp (ctlfd, rsp);
		        return;
	        }
	    }
	    SetHPBW ();
	    (void) sprintf (rsp, "%s: completed", cmd);
	}
	else {
	    (void) sprintf (rsp, "%s: rejected. Wrong number of args", cmd);
	}

	(void) send_rsp (ctlfd, rsp);
    }
    else if (!strncasecmp (cmd, "TRACK", 5)) {
	void process_apc_cmd (char *);

	process_apc_cmd (cmdstr);
    }
    else if (!strncasecmp (cmd, "STARTUP", 7)) {
	void process_apc_cmd (char *);

	process_apc_cmd (cmdstr);
    }
    else if (!strncasecmp (cmd, "MINICAL", 7)) {
	int rac_chan;
	char chan;

	if (racfd != -1) {
            if (sscanf (cmdstr, "%*s %c", &chan) == 1) {
		if (eac_chan[toupper(chan) - 65].freq > 0.0) {
		    if ((rac_chan = RacChan (toupper(chan) - 65)) > -1) {
		        sprintf (cmdstr, "minical %d", rac_chan + 1);
	                (void) send_cmd (racfd, cmdstr);
		    }
	            else {
	                (void) sprintf (rsp,
			    "%s: rejected. Check channel assignment.", cmd);
	                (void) send_rsp (ctlfd, rsp);
	            }
		}
		else {
	            (void) sprintf (rsp,
				"%s: rejected. EAC channel unassigned.", cmd);
	            (void) send_rsp (ctlfd, rsp);
	        }
	    }
	    else {
	        (void) sprintf (rsp,
				"%s: rejected. Incorrect channel arg", cmd);
	        (void) send_rsp (ctlfd, rsp);
	    }
	}
	else {
	    (void) sprintf (rsp, "%s: rejected. RAC not connected.", cmd);
	    (void) send_rsp (ctlfd, rsp);
	}
    }
    else if (!strncasecmp (cmd, "STOW", 4)) {
	void process_apc_cmd (char *);

	process_apc_cmd (cmdstr);
    }
    else if (!strncasecmp (cmd, "WAIT", 4)) {
	unsigned long wait_sec;

        if (sscanf (cmdstr, "%*s %d", &wait_sec) == 1) {
	    XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel),
				wait_sec * 1000, wait_completed, NULL);
	}
	else
	    (void) send_rsp (ctlfd, "wait: rejected. Arg error.");
    }
    else if (!strncasecmp (cmd, "no_op", 5)) {
    }
    else {
	(void) sprintf (rsp, "%s: rejected. Illegal command.", cmd);
	(void) send_rsp (ctlfd, rsp);
    }
}

/* ----------------------------------------------------------------------- */

void GetChannel (float frequency, POL pol, int *channel)
{
    int i;

    for (i = 0; i < MAX_CHAN; i++) {
        if ((int)rint (frequency * 1000) ==
			(int)rint (rcv_cnf.sky_freq[i] * 1000)
					&& pol == rcv_cnf.pol[i])
	    break;
    }

    *channel = i;
}

/* ----------------------------------------------------------------------- */

int EacChan (int rac_chan_no)
{
    int i;

    for (i = 0; i < MAX_CHAN; i++) {
	if (fabs (eac_chan[i].freq - rac_chan[rac_chan_no].freq) < .0001 &&
		!strcasecmp (eac_chan[i].pol, rac_chan[rac_chan_no].pol))
	    return (i);
    }

    return (-1);
}

int RacChan (int eac_chan_no)
{
    int i;

    for (i = 0; i < MAX_CHAN; i++) {
	if (fabs (rac_chan[i].freq - eac_chan[eac_chan_no].freq) < .0001 &&
		!strcasecmp (rac_chan[i].pol, eac_chan[eac_chan_no].pol))
	    return (i);
    }

    return (-1);
}

void process_rac_rsp (char *rsp)
{
    char  cmd[MAX_CMD_LEN];
    char  timestr[4][20];
    int   i, channel, n;
    float ifpwr[4];
    POL   pol = NONE;
    char  temp[20];

    float cf, smpl_tim, gain_in, lin, el, ut, tlna, tfollow,
          tphys, r1, r2, r3, r4, r5, r6, load_temp, systemp, nd,
          wxtemp, press, rel_hum, wind_vel, wind_dir, bw;
    int no_smpls, reads;
    char year[5], doy[4], hr[3], min[3], sec[3], time_str[20];

    FILE *OpenLog (String, String, String, String, String);
    if (sscanf (rsp, "%s", cmd) != 1)
	return;


    if (!strcasecmp (cmd, "if_pwr:")) {
	char        *chan_ptr, chan_id[8];
	char        timestring[16];
	time_t      current_time;
	int         i, j, stat;
	float       pri_tsys, secondary_tsys;
	MSQ  	    msq;

	if (resrc.debug)
	    (void)fprintf (stderr, "%s\n", rsp);

	/* Look for channel data in rac message. i is the rac chan */
	current_time = time ((time_t *) 0);
	pri_tsys = 0.0;
	secondary_tsys = 0.0;
	for (i = 0; i < MAX_CHAN; i++) {
	    (void)sprintf (chan_id, "CH%d", i + 1);
	    if ((chan_ptr = strstr (rsp, chan_id)) != NULL) {
	        sscanf (chan_ptr, "%*s %19s %f %f %3s", timestr[i],
			    &rac_chan[i].freq, &ifpwr[i], rac_chan[i].pol);

	        /* Got one. Load ALL matching eac channels */
	        for (j = 0; j < MAX_CHAN; j++) {
		    if (fabs (eac_chan[j].freq - rac_chan[i].freq) <.0001 &&
			!strcasecmp (eac_chan[j].pol, rac_chan[i].pol)) {

	                tsys[j] = ifpwr[i] * gain[j];
	                last_tsys_update[j] = current_time;

	                SetPol (rac_chan[i].pol, &pol);
	                calc_brsight_err (timestr[i],
					rac_chan[i].freq, ifpwr[i], pol);

			if (fabs (rcv_cnf.sky_freq[rcv_cnf.scan_ref_index] -
				rac_chan[i].freq) < .0001  && pol ==
				rcv_cnf.pol[rcv_cnf.scan_ref_index]) {
			    pri_tsys = tsys[j];
			    strncpy (timestring, timestr[i], 16);
			}
			else if (fabs (rcv_cnf.sky_freq
				[rcv_cnf.scan_secondary_index]-
				rac_chan[i].freq) < .0001  && pol ==
				rcv_cnf.pol[rcv_cnf.scan_secondary_index]) {
			    secondary_tsys = tsys[j];
			}


			/* Log if flag set */
	                if (logging_pwr[j]) {
	                    LogPwr (j, timestr[i], ifpwr[i],
			                i, rac_chan[i].freq,
					rac_chan[i].pol, gain[j]);
	                }
			else { /* Delete fp from previous log */
			    pwr_log[j] = NULL;
			}
		    }
	        }
	    }
	}
	msq.mtype = (long) MSQ_VARIABLE;
	(void)sprintf (msq.mtext, "%d %.4G %d %s %d %.4G",
			IF_PWR, pri_tsys, XSCAN_TIME_TAG, timestring,
			IF_PWR2, secondary_tsys);
        stat = msgsnd (snd_msqid, &msq, sizeof (msq.mtext), IPC_NOWAIT);

	for (i = 0; i < MAX_CHAN; i++) {
	    /* Clear tsys display if data stopped */
	    if (current_time - last_tsys_update[i] > 4)
	        tsys[i] = 0.0;
	}

    }
    else if (!strcasecmp (cmd, "ellipsoid_angle:")) {
	(void) sscanf (rsp, "%*s %f", &sr_pos.rotation);
    }
    else if (!strcasecmp (cmd, "ellipsoid_pos:")) {
	(void) sscanf (rsp, "%*s %1d", &rcv_cnf.feed_pos);
	SetMirrorLabel (rcv_cnf.feed_pos);
    }
    else if (!strcasecmp (cmd, "mirror:")) {
	time_t timetag;
	int    pos;

	if (ctlfd > 0) {
	    if (sscanf (rsp, "%*s %ld %1d", &timetag, &pos) == 2) {
	        (void) sprintf (rsp, "%s: completed", "feedpos");
	            (void) send_rsp (ctlfd, rsp);
	    }
	}
    }
    else if (!strcasecmp (cmd, "minical:")) {

        char header[512];
	char pol_str[4];

	if (strlen (rsp) < 25)
	    return;
	else {
	    if ((i = ant_index (&xrmd)) == ERROR)
	        return;

	    reads = 0;
	    tlna = 0.0;
	    tfollow = 0.0;
	    tphys = 0.0;
            (void) sscanf (rsp, "%*s %s %s %s %f %f %f %f %d %s %G %f \
                        %G %G %G %G %G %f %f",
                        doy, time_str, cmd, &systemp, &cf, &bw,
			&smpl_tim, &no_smpls, pol_str, &gain_in, &lin,
			&r1, &r2, &r3,
                        &r4, &r5, &load_temp, &nd);

	    (void) strftime (year, sizeof (year), "%Y", gmt);
            (void) sprintf (header, "\nYear %s\nDss %d\nFrequency %.3f\n"
                    "Bandwidth %f\nSample_time(sec) %.3f\nSamples/reading %d\n"
                    "Reads/point %d\n\nDOY UT Freq Pol Gain Lin El UT T(lna) "
                    "T(follow) Tphys R1 R2 R3 R4 R5 R6 T(load) Tsys "
                    "Tdiode Press WxTemp Hum Wind_vel Wind_dir\n",
                    year, antno, cf, bw, smpl_tim, no_smpls, reads);

	    SetPol (pol_str, &pol);
	    GetChannel(cf, pol, &channel);

	    if (channel == MAX_CHAN) {
		(void) warning (toplevel, "Minical frequency does not match\n"
				   "an XAnt channel --- Data not logged.");
		return;
	    }


            if (!mini_log[channel])
                if ((mini_log[channel] = OpenLog
                        (resrc.log_dir, "m", cmd, pol_str, header)) == NULL)
                    return;

            gain[channel] = gain_in;

	    r6=0.0;
            el =  xrmd.fea[i].angle.el;
            strncpy (hr, time_str, 2);
            strncpy (min, time_str+3, 2);
            strncpy (sec, time_str+6, 2);
            ut = atof(hr) + atof(min) / 60.0 + atof(sec) / 3600.0;
            wxtemp = xrmd.wthr.temp;
            press = xrmd.wthr.press;
            rel_hum = xrmd.wthr.hum;
            wind_vel = xrmd.wthr.wind_sp;
            wind_dir = xrmd.wthr.wind_dir;
            (void) fprintf (mini_log[channel],
              "%s %s %.3f %s %.4G %.4f %.3f %.3f %.3f %.3f %.3f %.4G %.4G %.4G "
                "%.4G %.4G %.4G %.2f %.3f %.2f %.1f %.1f %.1f %.1f %.0f\n",
                  doy, time_str, rcv_cnf.sky_freq[channel], pol_str,
		  gain_in, lin, el, ut, tlna, tfollow,
                  tphys, r1, r2, r3, r4, r5, r6, load_temp, systemp, nd, press,
                  wxtemp, rel_hum, wind_vel, wind_dir);
            (void) fflush (mini_log[channel]);
        }
        if (ctlfd != -1)
            (void) send_rsp (ctlfd, "minical: completed");
    }
    else if (ctlfd != -1)
        (void) send_rsp (ctlfd, rsp);
}

/* ----------------------------------------------------------------------- */

void check_on_src (int  msg_id)
{
    boolean reset = true;

    if (is_onpoint (!reset, 0)) {
	char rsp[MAX_RSP_LEN];

	if (trk_cnf.mode == SIDEREAL)
	    (void) sprintf (rsp, "onpoint: Tracking %f %f "
	    			 "%7.4f %7.4f %7.4f %7.4f %7.4f %7.4f",
				 trk_src.ra, trk_src.dec,
				 m_offset.az_pos_offset,
				 m_offset.el_pos_offset,
				 m_offset.xel_pos_offset,
				 m_offset.ha_pos_offset,
				 m_offset.dec_pos_offset,
				 m_offset.xdec_pos_offset);
	else
	    (void) sprintf (rsp, "onpoint: Tracking!");

	if (send_rsp (ctlfd, rsp) <= 0) {
	    (void) net_close (ctlfd);
	    ctlfd = ERROR;
	}
    }
    else {
	if (send_rsp (ctlfd, "onpoint: Slewing...") <= 0) {
	    (void) net_close (ctlfd);
	    ctlfd = ERROR;
	}
	if (msg_id == RCTL_CMD && ctlfd != ERROR)
	    XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel), 3000,
			     check_on_src, msg_id);
    }
}

/* ----------------------------------------------------------------------- */

void check_on_pos (MOV_POS  *movp)
{
    int  i;
    char odstr[MAX_OD_LEN];
    boolean reset = true;

    static int reqcount = 0;

    if ((i = ant_index (&xrmd)) == ERROR)
	return;

    if (rate1 < 0.0005 && rate2 < 0.0005) {
	if ((movp->coord == EL &&
			fabs (movp->pos - xrmd.fea[i].asc.el) <= 0.001) ||
	    (movp->coord == AZ &&
			fabs (movp->pos - xrmd.fea[i].asc.az) <= 0.001)) {
	    if (send_rsp (ctlfd, "move: completed") <= 0) {
		(void) net_close (ctlfd);
		ctlfd = ERROR;
	    }
	    reqcount = 0;
	    return;
	}
	else if (reqcount++ < 2)
	    /*
	    (void) send_od (sgwfd, linkno, antpc, "ACTL REQPOS");
	    */
	    ;
	else {
	    if (movp->coord == EL)
		(void) sprintf (odstr, "MOVE EL=%08.4f", movp->pos);
	    else
		(void) sprintf (odstr, "MOVE AZ=%08.4f", movp->pos);

	    (void) send_od (sgwfd, linkno, antpc, odstr);
	    (void)is_onpoint (reset, SOURCE_CHANGE_WAIT);
	    reqcount = 0;
	}
    }
    if (ctlfd != ERROR)
	XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel), 5000,
			 check_on_pos, movp);
}

/* ----------------------------------------------------------------------- */

void process_anteq (char *cmdstr)
{
    char  cmdbuf[MAX_CMD_LEN];
    char  odstr[MAX_OD_LEN];
    char  rsp[MAX_RSP_LEN];
    char  line[MAX_LINE_LEN];
    char  fname[64];
    char  errstr[80];
    char *subsys, *tokptr, *l, *s;
    char *lasts = NULL;
    FILE *fp;
    int   i = 0, j;

    (void) sprintf (fname, "%s/%s", resrc.cfg_dir, ANTCMD_FNAME);

    if ((fp = fopen (fname, "r")) == NULL) {
	(void) sprintf (errstr, "Cannot open %s\n"
				"Direct antenna commands rejected.", fname);
	(void) warning (toplevel, errstr);
	(void) sprintf (rsp, "antenna: rejected. Can't open %s", fname);
	(void) send_rsp (ctlfd, rsp);
	return;
    }

    (void) strncpy (cmdbuf, cmdstr, MAX_CMD_LEN);
    cmdbuf[MAX_CMD_LEN - 1] = '\0';
    while (cmdbuf[i] != '\0') cmdbuf[i++] = toupper (cmdbuf[i]);

    s = cmdbuf + 7;
    while (isspace (*++s));
    (void) strncpy (odstr, s, MAX_OD_LEN);
    odstr[MAX_OD_LEN - 1] = '\0';

    if (strtok_r (s, " ", &lasts) == NULL) {
	(void) send_rsp (ctlfd, "antenna: rejected. Missing direct command.");
	return;
    }
    for (i=0; i<2 && (tokptr = strtok_r
			((char *)NULL, ",= \n", &lasts))!=NULL; i++) {
	j = 0;
	while (deny[j] != NULL)
	    if (!strcmp (tokptr, deny[j++])) {
		(void) sprintf (rsp, "antenna: rejected. \"%s\" not allowed",
				odstr);
		(void) send_rsp (ctlfd, rsp);
		return;
	    }
    }
    (void) strcpy (cmdbuf, odstr);
    j = 0;

    while (fgets (line, MAX_LINE_LEN, fp) && j < MAX_ALLOW_CMD-1) {
	if (line[0] == '#') continue;
	for (i = strlen (line) - 1; i >= 0 && isspace (line[i]); i--);
	if (i < 0) continue;
	line[i+1] = '\0';
	l = line;
	allow[j++] = s = (char *) malloc (strlen (line) + 1);
	while (*s++ = toupper (*l++)) {
	    if (isspace (*(l-1)))
		while (isspace (*l))
		    l++;
	}
    }
    allow[j] = NULL;
    if (fp != NULL)
	fclose (fp);

    j = 0;
    while (allow[j] != NULL && strstr (cmdbuf, allow[j]) == NULL)
	j++;

    if (allow[j] == NULL) {
	(void) sprintf (rsp, "antenna: rejected. \"%s\" not allowed", odstr);
	(void) send_rsp (ctlfd, rsp);
    }
    else {
	subsys = strtok_r (cmdbuf, " \n", &lasts);
	s = odstr + strlen (subsys) - 1;
	while (isspace (*++s));

	if (!strcmp (subsys, "AP"))
	    (void) send_od (sgwfd, linkno, antpc, s);
	else if (!strcmp (subsys, "RC"))
	    (void) send_od (sgwfd, linkno, RCC_PC, s);
	else if (!strcmp (subsys, "V"))
	    (void) send_od (sgwfd, linkno, DSPV_PC, s);

	(void) send_rsp (ctlfd, "antenna: completed");
    }
}

void SetPol (char *pol_str, POL *pol)
{
    if (!strcasecmp (pol_str, "RCP"))
        *pol = RCP;
    else if (!strcasecmp (pol_str, "LCP"))
        *pol = LCP;
    else {
	*pol = NONE;
    }
}

void CheckAntMode (APC_CMD_DATA *data_p)
{
    char rsp[MAX_RSP_LEN];

    /* If the antenna was already tracking, you'll get TRACKING instead of
    HOLD */

    if (xrmd.fea[data_p->fea_index].apc.ant_op_state == data_p->mode) {
        (void) sprintf (rsp, "%s: completed", data_p->cmd);
        (void) send_rsp (ctlfd, rsp);
    }
    else if (data_p->mode == HOLD &&
        	xrmd.fea[data_p->fea_index].apc.ant_op_state == TRACKING) {
        (void) sprintf (rsp, "%s: completed", data_p->cmd);
        (void) send_rsp (ctlfd, rsp);
    }
    else
	(void)XtAppAddTimeOut (XtWidgetToApplicationContext (toplevel),
					1000, CheckAntMode, data_p);
}

void process_apc_cmd (char *cmd)
{
    if (sgwfd != -1) {
	static APC_CMD_DATA data;
	XmString xmlabel;
	char     *label;
	int i;

	if ((i = ant_index (&xrmd)) == ERROR)
	    return;

	data.fea_index = i;
	(void)strcpy (data.cmd, cmd);

        if (!strcasecmp (cmd, "startup")) {
	    if (xrmd.fea[i].apc.estop == CLEAR) {
                switch (xrmd.fea[i].apc.ant_op_state) {
			case HOLD:
	                    data.mode = HOLD;
			    break;
			case TRACKING:
	                    data.mode = TRACKING;
			    break;
			default:
			    /* Supresses resume warning popup */
	                    hold_warning = true;
	                    ServoCtrl (topform->trk_btn, NULL, NULL);
	                    data.mode = HOLD;
		}
	        CheckAntMode (&data);
	    }
	    else
	        (void) send_rsp (ctlfd, "startup: rejected.  Reset antenna");
        }
        else if (!strcasecmp (cmd, "track")) {
            switch (xrmd.fea[i].apc.ant_op_state) {
		case HOLD:
	            ServoCtrl (topform->trk_btn, NULL, NULL);
                    data.mode = TRACKING;
                    CheckAntMode (&data);
		    break;
		case TRACKING:
                    data.mode = TRACKING;
                    CheckAntMode (&data);
		    break;
		default:
	            (void) send_rsp (ctlfd, "track: rejected. Not started?");
	    }
        }
        else if (!strcasecmp (cmd, "stop")) {
            (void)send_od (sgwfd, linkno, antpc, "STOP");
	    data.mode = STOP;
	    CheckAntMode (&data);
        }
        else if (!strcasecmp (cmd, "stow")) {
            (void)send_od (sgwfd, linkno, antpc, "STOW");
	    data.mode = STOW;
	    CheckAntMode (&data);
        }
    }
    else {
	char rsp[MAX_RSP_LEN];

	(void) sprintf (rsp, "%s: rejected. Gateway not connected.", cmd);
	(void) send_rsp (ctlfd, rsp);
    }
}

void proc_xscan (char *integ, char *scan_axes, char *width, char *rate)
{
    int status;

    if (!strcasecmp (scan_axes, "xdecdec"))
        XtVaSetValues (xscan_entry->xscan_axes, XmNmenuHistory,
					xscan_entry->xdec_pb, NULL);
    else if (!strcasecmp (scan_axes, "xelel"))
        XtVaSetValues (xscan_entry->xscan_axes, XmNmenuHistory,
					xscan_entry->xel_pb, NULL);
    else {
        (void) send_rsp (ctlfd, "xscan: rejected. Unknown axes");
	return;
    }

    XmTextFieldSetString (xscan_entry->xscan_width, width);
    XmTextFieldSetString (xscan_entry->xscan_avg, integ);
    XmTextFieldSetString (xscan_entry->xscan_rate, rate);

    status = ActivateXScan (NULL, 0, NULL);

    switch (status) {
        case -1:
            (void) send_rsp (ctlfd, "xscan: rejected. Internal error");
	    break;
	case -2:
	    (void) send_rsp (ctlfd, "xscan: rejected. No ant. No freq");
	    break;
	case -3:
	    (void) send_rsp (ctlfd, "xscan: rejected. Assign antenna");
	    break;
	case -4:
	    (void) send_rsp (ctlfd, "xscan: rejected. Set frequency");
	    break;
	case -5:
	    (void) send_rsp (ctlfd, "xscan: rejected. Bad width (2-40 ok");
	    break;
	case -6:
	    (void) send_rsp (ctlfd, "xscan: rejected. Bad rate");
	    break;
	case -7:
	    (void) send_rsp (ctlfd, "xscan: rejected. Bad Avg (1-20 ok");
    }
}

void proc_pscanreq (char *integ, char *scan_axes, char *baseline_offset)
{
    if (atoi (integ) > 0 && atoi (integ) <= 1000) {
        if (!strcasecmp (scan_axes, "xdecdec")) {
            XtVaSetValues (fivept_entry->fivept_axes, XmNmenuHistory,
				fivept_entry->xdec_pb, NULL);
	    XmTextFieldSetString (fivept_entry->fivept_offsrc,
							baseline_offset);
	    XmTextFieldSetString (fivept_entry->fivept_duration, integ);

	    /* Set the global scan_type so scan button will terminate
	       current scan and 5 point data will reach calc routine */

	    scan_type_cb (NULL, "FivePoint", NULL);

	    ActivateBoresight ();

        /*
	else if (!strcasecmp (scan_axes, "azel"))
	    ActivateBoresight ();
	*/

	}
	else
	    (void) send_rsp (ctlfd, "pscan: rejected. Unknown axes");
    }
    else
        (void) send_rsp (ctlfd,
			"pscan: rejected. Number of samples out of range");
}

void process_xscanreq (char *cmdstr)
{
    int  argcount;
    char integ[MAX_CMD_LEN];
    char baseline_offset[MAX_CMD_LEN];
    char scan_axes[MAX_CMD_LEN];
    char rate[MAX_CMD_LEN];

    argcount = sscanf (cmdstr, "%*s %20s %20s %20s %20s",
				scan_axes, integ, baseline_offset, rate);
    if (argcount == 3 ) {
	/* Baseline offset omitted and contains rate */
	strcpy (rate, baseline_offset);
	strcpy (baseline_offset, "7.0");
	proc_xscan (integ, scan_axes, baseline_offset, rate);
    }
    else if (argcount == 4 ) {
	proc_xscan (integ, scan_axes, baseline_offset, rate);
    }
    else
        (void) send_rsp (ctlfd, "xscan: rejected. Missing args");
}

void process_pscanreq (char *cmdstr)
{
    int  argcount;
    char integ[MAX_CMD_LEN];
    char baseline_offset[MAX_CMD_LEN];
    char scan_axes[MAX_CMD_LEN];
    char rate[MAX_CMD_LEN];

    argcount = sscanf (cmdstr, "%*s %20s %20s %20s",
				scan_axes, integ, baseline_offset);
    if (argcount == 2 ) {
        strcpy (baseline_offset, "7.0");
        proc_pscanreq (integ, scan_axes, baseline_offset);
    }
    else if (argcount == 3 ) {
        proc_pscanreq (integ, scan_axes, baseline_offset);
    }
    else
        (void) send_rsp (ctlfd, "pscan: rejected. Missing args");
}

void process_scanreq (char *cmdstr)
{
    int  argcount;
    char integ[MAX_CMD_LEN];
    char baseline_offset[MAX_CMD_LEN];
    char scan_axes[MAX_CMD_LEN];
    char rate[MAX_CMD_LEN];
    char type[MAX_CMD_LEN];

    argcount = sscanf (cmdstr, "%*s %20s %20s %20s %20s", type,
				scan_axes, integ, baseline_offset);
    if (argcount == 3 || argcount == 4){
	if (!strcasecmp (type, "5point")) {
            if (argcount == 3 ) {
                strcpy (baseline_offset, "7.0");
                proc_pscanreq (integ, scan_axes, baseline_offset);
            }
            else {
                proc_pscanreq (integ, scan_axes, baseline_offset);
            }
	}
	else
            (void) send_rsp (ctlfd, "pscan: rejected. Bad type");
    }
    else
        (void) send_rsp (ctlfd, "pscan: rejected. Missing args");
}

void lookup_source (char *srcname)
{
    void TrackNewSrc (Widget, PLOT_DATA *, XmPushButtonCallbackStruct *);

    FILE  *fp;
    char  line[MAX_LINE_LEN];
    char  errstr[64];
    XmFileSelectionBoxCallbackStruct cbs;

    (void)sprintf (line, "find /opt/rmc/predicts -follow -name %s "
			"-print > /home/ops/tmp/predict_temp", srcname);
    (void)system (line);

    if ((fp = fopen ("/home/ops/tmp/predict_temp", "r")) == NULL) {
        (void) sprintf (errstr, "Cannot open %s\n", "predict_temp");
        (void) warning (toplevel, errstr);
        return;
    }

    if (fgets (line, MAX_LINE_LEN, fp) != NULL) {
	XmString xms = XmStringCreateLtoR(line,
			(XmStringCharSet)XmFONTLIST_DEFAULT_TAG);
	cbs.value = xms;
	select_src (&cbs);
	XmStringFree (xms);
	TrackNewSrc (topform->trk_btn, &plot_data, NULL);
    }
    else {
        (void) sprintf (errstr, "Cannot find %s\n", srcname);
	(void) fprintf (stderr, errstr);
        (void) warning (toplevel, errstr);
    }
}

void precess_source (char *srcname, float ra, float dec, char *epoch)
{
    double outpos[6], inpos[6], jdu, jde;
    int    iin, iout;
    char   date[16];

    (void) strftime (date, sizeof(date), "%Y%m%e%k%M%S", gmt);
    if (!strcmp (epoch, "J2000")) {
	iin = 105;
    }
    else if (!strcmp (epoch, "B1950")) {
	iin = 101;
    }

    strcpy (new_src.epoch, epoch);

    iout= 102;
    jdu = julian_date (date);
    inpos[0] = 1.0;
    inpos[1] = (double) dec;
    inpos[2] = (double) ra;
    inpos[3] = 0.0;
    inpos[4] = 0.0;
    inpos[5] = 0.0;
    eclndr_ (&jde, "JDE", &jdu, "JDU", 3, 3);
    etrans_ (outpos, &iout, inpos, &iin, &jde);

    (void) memset (&new_src, 0, sizeof (new_src));
    (void) strncpy (new_src.name, srcname, NAME_LEN - 1);
    new_src.ra  = outpos[2];
    new_src.dec = outpos[1];
    new_src.a1  = new_src.ra;
    new_src.a2  = new_src.dec; 
    trk_cnf.next_mode = SIDEREAL;
    TrackNewSrc (topform->trk_btn, &plot_data, NULL);
}

void load_source (char *srcname, float pra, float pdec, TRK_MODE mode)
{
    char p_str[64];
    char ret_str[64];
    char sign;

    (void)sprintf (p_str, "%f", pra);
    ret_str[0] = '\0';
    if (strchr (p_str, '+') == NULL &&
                        (strcspn (p_str, ".") == 6 ||
                                strcspn (p_str, ".") == 5)) {
        ConvRa (p_str, ret_str);
    }
    else if (strchr (p_str, '+') == NULL &&
                                strcspn (p_str, ".") < 4)
        strcpy (ret_str, p_str);
    else if (strcspn (p_str, "+") == 0 &&
                        (strcspn (p_str, ".") == 7 ||
                                strcspn (p_str, ".") == 6)) {
        ConvRa (p_str + 1, ret_str);
    }
    else if (strcspn (p_str, "+") == 0 &&
                                strcspn (p_str, ".") < 5)
        strcpy (ret_str, p_str);
    else {
        (void) send_rsp (ctlfd, "source: rejected. Can't interpret Ra.");
        return;
    }

    pra = atof (ret_str);

    (void)sprintf (p_str, "%f", pdec);
    ret_str[0] = '\0';
    if (strchr (p_str, '+') == NULL &&
                        strchr (p_str, '-') == NULL &&
                        (strcspn (p_str, ".") == 6 ||
                                strcspn (p_str, ".") == 5)) {
        sign = '+';
        ConvDec (sign, p_str, ret_str);
    }
    else if (strchr (p_str, '+') == NULL &&
                        strchr (p_str, '-') == NULL &&
                                strcspn (p_str, ".") < 3)
        strcpy (ret_str, p_str);
    else if ((strcspn (p_str, "+") == 0 ||
                        strcspn (p_str, "-") == 0) &&
                        (strcspn (p_str, ".") == 7 ||
                                strcspn (p_str, ".") == 6)) {
        sign = p_str[0];
        ConvDec (sign, p_str + 1, ret_str);
    }
    else if ((strcspn (p_str, "+") == 0 ||
                        strcspn (p_str, "-") == 0) &&
                                strcspn (p_str, ".") < 4)
        strcpy (ret_str, p_str);
   else {
        (void) send_rsp (ctlfd, "source: rejected. Can't interpret Dec.");
        return;
    }

    pdec = atof (ret_str);

    (void) memset (&new_src, 0, sizeof (new_src));
    (void) strncpy (new_src.name, srcname, NAME_LEN -1);
    new_src.ra  = pra;
    new_src.dec = pdec;
    new_src.a1  = pra;
    new_src.a2  = pdec;
    trk_cnf.next_mode = mode;
    TrackNewSrc (topform->trk_btn, &plot_data, NULL);
}

void wait_completed ()
{
    if (ctlfd != -1)
        (void) send_rsp (ctlfd, "wait: completed");
}

void set_channel (char *s)
{
    int  rac_chan, i;
    int loopno = 0;
    char chan;
    float freq;
    char pol_str[4], freq_str[16];
    POL  pol;
    XmString xms;

    if (sscanf (s, "%*s %c %15s %3s",
			&chan, freq_str, pol_str) == 3) {
	freq = (float)atof (freq_str);
	SetPol (pol_str, &pol);
	i = 0;
	while ((pol_str[i++] = toupper (pol_str[i])) != '\0');
	if (pol == NONE) {
            (void) send_rsp (ctlfd, "assign: rejected. Polarization error.");
	}
	else if (toupper(chan) >= 'A' && toupper(chan) <= 'D') {
	    PopupRcvConfigDialog (toplevel, NULL, NULL);
	    switch (toupper(chan)) {
	        case 'A':
		    XmTextFieldSetString (cfg_rcv->freq_a, freq_str);
		    if (pol == RCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->rcp_a, True, True);
		    else if (pol == LCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->lcp_a, True, True);
		    ConfigureRcv (toplevel, NULL, NULL);
                    (void) send_rsp (ctlfd, "assign: completed");
		    break;
	        case 'B':
		    XmTextFieldSetString (cfg_rcv->freq_b, freq_str);
		    if (pol == RCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->rcp_b, True, True);
		    else if (pol == LCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->lcp_b, True, True);
		    ConfigureRcv (toplevel, NULL, NULL);
                    (void) send_rsp (ctlfd, "assign: completed");
		    break;
	        case 'C':
		    XmTextFieldSetString (cfg_rcv->freq_c, freq_str);
		    if (pol == RCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->rcp_c, True, True);
		    else if (pol == LCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->lcp_c, True, True);
		    ConfigureRcv (toplevel, NULL, NULL);
                    (void) send_rsp (ctlfd, "assign: completed");
		    break;
	        case 'D':
		    XmTextFieldSetString (cfg_rcv->freq_d, freq_str);
		    if (pol == RCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->rcp_d, True, True);
		    else if (pol == LCP)
			XmToggleButtonGadgetSetState
					(cfg_rcv->lcp_d, True, True);
		    ConfigureRcv (toplevel, NULL, NULL);
                    (void) send_rsp (ctlfd, "assign: completed");
	    }

	    #if(0)
	    if (rcv_cnf.feed_pos == P && racfd > 0) {

	        /* Request mirror position */

	        (void) send_cmd (racfd, "Ellipsoid_Pos");

	        /* set mirror label */

	        while (rcv_cnf.feed_pos == P && loopno < 10) {
		    sleep (1);
		    loopno++;
	        }

	        SetMirrorLabel (rcv_cnf.feed_pos);
	    }
	    #endif
	}
	else
            (void) send_rsp (ctlfd, "assign: rejected. Bad channel");
    }
    else
        (void) send_rsp (ctlfd, "assign: rejected. Wrong number of args.");
}
