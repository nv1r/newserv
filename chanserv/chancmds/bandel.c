/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: bandel
 * CMDALIASES: unban
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 2
 * CMDDESC: Removes a single ban from a channel.
 * CMDFUNC: csc_dobandel
 * CMDPROTO: int csc_dobandel(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <channel> <ban>
 * CMDHELP: Removes the specified persistent or channel ban, where:
 * CMDHELP: channel - the channel to use
 * CMDHELP: ban     - either a ban mask (nick!user@host), or #number (see BANLIST)
 * CMDHELP: Removing channel bans requires operator (+o) access on the named channel.
 * CMDHELP: Removing persistent bans requires master (+m) access on the named channel.
 */

#include "../chanserv.h"
#include "../../nick/nick.h"
#include "../../lib/flags.h"
#include "../../lib/irc_string.h"
#include "../../channel/channel.h"
#include "../../parser/parser.h"
#include "../../irc/irc.h"
#include "../../localuser/localuserchannel.h"
#include <string.h>
#include <stdio.h>

int csc_dobandel(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban **rbh, *rbp;
  chanban *cbp;
  regchan *rcp;
  chanban *theban=NULL;
  modechanges changes;
  int i,banid=0;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unban");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "unban", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  /* OK, let's see what they want to remove.. */
  if (*cargv[1]=='#') {
    /* Remove by ID number */
    if (!(banid=strtoul(cargv[1]+1, NULL, 10))) {
      chanservstdmessage(sender, QM_UNKNOWNBAN, cargv[1], cip->name->content);
      return CMD_ERROR;
    }
  } else {
    /* Remove by ban string */
    theban=makeban(cargv[1]);
  }
   
  i=0;
  for (rbh=&(rcp->bans);*rbh;rbh=&((*rbh)->next)) {
    i++;
    if ((banid  && i==banid) ||
	(theban && banequal(theban, (*rbh)->cbp))) {
      /* got it - they will need master access to remove this */
      rbp=*rbh;
      
      if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "unban", 0, 0))
	return CMD_ERROR;
      
      chanservstdmessage(sender, QM_REMOVEDPERMBAN, bantostring(rbp->cbp), cip->name->content);
      if (cip->channel) {
	localsetmodeinit(&changes, cip->channel, chanservnick);    
	localdosetmode_ban(&changes, bantostring(rbp->cbp), MCB_DEL);
	localsetmodeflush(&changes, 1);
      }
      
      /* Remove from database */
      csdb_deleteban(rbp);
      /* Remove from list */
      (*rbh)=rbp->next;
      /* Free ban/string and actual regban */
      freesstring(rbp->reason);
      freechanban(rbp->cbp);
      freeregban(rbp);

      if (theban)
	freechanban(theban);

      return CMD_OK;
    }
  }
  
  /* If we've run out of registered bans, let's try channel bans */
  if (cip->channel && cip->channel->bans) {
    for (cbp=cip->channel->bans;cbp;cbp=cbp->next) {
      for (rbp=rcp->bans;rbp;rbp=rbp->next) {
	if (banequal(rbp->cbp,cbp))
	  break;
      }
      
      if (rbp)
	continue;
      
      i++;
      if ((banid  && (i==banid)) ||
	  (theban && banequal(theban, cbp))) {
        char *banmask = bantostring(cbp);

	/* got it - this is just a channel ban */
	chanservstdmessage(sender, QM_REMOVEDCHANBAN, banmask, cip->name->content);
	localsetmodeinit(&changes, cip->channel, chanservnick);
	localdosetmode_ban(&changes, banmask, MCB_DEL);
	localsetmodeflush(&changes, 1);

	if (theban)
	  freechanban(theban);
	
	return CMD_OK;
      }
    }
  }
	
  chanservstdmessage(sender, QM_UNKNOWNBAN, cargv[1], cip->name->content);

  return CMD_OK;
} 
