/*******************************************************************************

  Implementation of VDP protocol for IEEE 802.1 Qbg Ratified Standard
  (c) Copyright IBM Corp. 2013

  Author(s): Thomas Richter <tmricht at linux.vnet.ibm.com>

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#ifndef	QBG_VDP22_H
#define	QBG_VDP22_H

/*
 * Defintion of VDP22 data structures:
 * For IEEE 802.1Qbg ratified standard the Virtual Station Information (VSI)
 * in the VDP22 protocol is maintained as a queue with one entry per interface
 * supporting VDP22.
 * Each interface is the anchor for a list of active VSI manager identifiers.
 * All VSIs with the same
 * - manager-id
 * - type-id and type version
 * - VSI format type and VSI id
 * - filter info format
 * are grouped into one entry. This means if two VSIs differ only in the
 * filter info format (for example), two seperate VSI list elements will be
 * allocated and queued in the VSI queue of that interface name.
 *
 * Each VSI node maintains the VDP22 state machine for that association.
 *
 * Supported operations:
 * - Find a VSI entry given the search criterias:
 *   manager-id, type-id, type-version, VSI-format, VSI-ID and filter-info
 *   (data and format).
 * - Add new filter data to a matching VSI entry (and filter not already
 *   active).
 * - Remove filter data from a matching VSI entry.
 * - Add a new VSI node when no VSI match is found.
 * - Remove a VSI node when no filter entry is active anymore.
 */

#include	<sys/queue.h>
#include	<linux/if_ether.h>
#include	<linux/if_link.h>

enum vdp22_role {		/* State for VDP22 bridge processing */
	VDP22_BRIDGE = 1,	/* Bridge role */
	VDP22_STATION		/* State role */
};

/*
 * Define VDP22 filter info formats.
 */
enum vdp22_ffmt {
	 VDP22_FFMT_VID = 1,
	 VDP22_FFMT_MACVID,
	 VDP22_FFMT_GROUPVID,
	 VDP22_FFMT_GROUPMACVID
};

/*
 * Define VDP22 VSI Profile modes.
 */
enum vdp22_modes {
	VDP22_ENDTLV = 0,
	VDP22_PREASSOC,
	VDP22_PREASSOC_WITH_RR,
	VDP22_ASSOC,
	VDP22_DEASSOC,
	VDP22_MGRID,
	VDP22_OUI = 0x7f
};

enum vdp22_cmdresp {			/* VDP22 Protocol command responses */
	VDP22_RESP_SUCCESS = 0,		/* Success */
	VDP22_RESP_INVALID_FORMAT = 1,
	VDP22_RESP_NO_RESOURCES = 2,
	VDP22_RESP_NO_VSIMGR = 3,	/* No contact to VSI manager */
	VDP22_RESP_OTHER = 4,		/* Other reasons */
	VDP22_RESP_NOADDR = 5,		/* Invalid VID, MAC, GROUP etc */
	VDP22_RESP_DEASSOC = 252,	/* Deassoc response */
	VDP22_RESP_TIMEOUT = 253,	/* Timeout response */
	VDP22_RESP_KEEP = 254,		/* Keep response */
	VDP22_RESP_NONE = 255		/* No response returned so far */
};

enum {
	VDP22_MGRIDSZ = 16,		/* Size of manager identifier */
	VDP22_IDSZ = 16,		/* Size of vsi identifier */
	VDP22_ID_IP4 = 1,		/* VSI ID is IPv4 address */
	VDP22_ID_IP6,			/* VSI ID is IPv6 address */
	VDP22_ID_MAC,			/* VSI ID is IEEE 802 MAC address */
	VDP22_ID_LOCAL,			/* VSI ID is locally defined */
	VDP22_ID_UUID,			/* VSI ID is RFC4122 UUID */
	VDP22_MTOBIT = 16,		/* VSI indicate migration to */
	VDP22_SUSPBIT = 32		/* VSI indicate migration from */
};

struct vsi_origin {		/* Originator of VSI request */
	pid_t req_pid;		/* PID of requester for VSI */
	unsigned long req_seq;	/* Seq # of requester for VSI */
};

/*
 * Generic filter data. Some field are unused, this depends
 * on the filter info format value (see enum above).
 */
struct fid22 {				/* Filter data: GROUP,MAC,VLAN entry */
	unsigned long grpid;		/* Group identifier */
	unsigned char mac[ETH_ALEN];	/* MAC address */
	unsigned short vlan;		/* VLAN idenfier */
	unsigned char ps;		/* PS field */
	unsigned char pcp;		/* PCP valid when PS true */
	struct vsi_origin requestor;
};

/*
 * VSI information. One node per matching entry (same mgrid, type_id, type_ver,
 * id_fmt, id and fif). Filter data can be added and removed.
 */
enum vsi22_flags {			/* Flags (or'ed in) */
	VDP22_BUSY = 1,			/* This node is under work */
	VDP22_DELETE_ME = 2,		/* Deallocate this node */
	VDP22_RETURN_VID = 4,		/* Return wildcard vlan id */
	VDP22_NOTIFY = 8,		/* Send netlink message to requestor */
	VDP22_NLCMD = 16		/* Netlink command pending */
};

struct vdp22smi {		/* Data structure for VDP22 state machine */
	int state;		/* State of VDP state machine for VSI */
	bool kato;		/* VSI KA ACK timeout hit for this VSI */
	bool ackreceived;	/* VSI ACK received for this VSI */
	bool acktimeout;	/* VSI ACK timeout hit for this VSI */
	bool localchg;		/* True when state needs change */
	bool deassoc;		/* True when deassoc received from switch */
	bool txmit;		/* True when packed TLV transmitted */
	bool resp_ok;		/* True when acked TLV received and match ok */
	int txmit_error;	/* != 0 error code from transmit via ECP */
};

struct vsi22 {
	LIST_ENTRY(vsi22) node;		/* Node element */
	unsigned char mgrid[VDP22_MGRIDSZ];	/* Manager identifier */
	unsigned char cc_vsi_mode;	/* currently confirmed VSI mode */
	unsigned char vsi_mode;		/* VSI mode: ASSOC, PREASSOC, etc */
	unsigned char resp_vsi_mode;	/* Responsed VSI mode: ASSOC, etc */
	unsigned char status;		/* Status, Request/Response */
	unsigned char hints;		/* Indicate migration/suspend */
	unsigned long type_id;		/* Type identifier */
	unsigned char type_ver;		/* Type version */
	unsigned char vsi_fmt;		/* Format of VSI identifier */
	unsigned char vsi[VDP22_IDSZ];	/* VSI identifier */
	unsigned char fif;		/* Filter info format */
	unsigned short no_fdata;	/* Entries in filter data */
	struct fid22 *fdata;		/* Filter data variable length */
	struct vdp22 *vdp;		/* Back pointer to VDP head */
	unsigned long flags;		/* Flags, see above */
	struct vdp22smi smi;		/* State machine information */
};

struct vdp22 {				/* Per interface VSI/VDP data */
	LIST_ENTRY(vdp22) node;		/* Node element */
	char ifname[IFNAMSIZ + 1];	/* Interface name */
	unsigned char ecp_retries;	/* # of ECP module retries */
	unsigned char ecp_rte;		/* ECP module retry timeout exponent */
	unsigned char vdp_rwd;		/* Resource wait delay exponent */
	unsigned char vdp_rka;		/* Reinit keep alive exponent */
	unsigned char gpid;		/* Supports group ids in VDP */
	unsigned char myrole;		/* Station or bridge role */
	unsigned char br_down;		/* True when bridge down */
	unsigned short input_len;	/* Length of input data from ECP */
	unsigned char input[ETH_DATA_LEN];	/* Input data from ECP */
	LIST_HEAD(vsi22_head, vsi22) vsi22_que;	/* Active VSIs */
};

struct vdp22_user_data {		/* Head for all VDP data */
	LIST_HEAD(vdp22_head, vdp22) head;
};

struct lldp_module *vdp22_register(void);
void vdp22_unregister(struct lldp_module *);
void vdp22_start(const char *, int);
void vdp22_showvsi(struct vsi22 *p);
void vdp22_stop(char *);
int vdp22_from_ecp22(struct vdp22 *);
int vdp22_query(const char *);
int vdp22_addreq(struct vsi22 *, struct vdp22 *);
int vdp22_nlback(struct vsi22 *);
struct vsi22 *vdp22_copy_vsi(struct vsi22 *);
void vdp22_listdel_vsi(struct vsi22 *);
int vdp22br_resources(struct vsi22 *, int *);
int vdp22_local2str(const u8 *, char *, size_t);
#endif