/* $Copyright: * * Copyright 1998-2000 by the Massachusetts Institute of Technology. *  * All rights reserved. *  * Export of this software from the United States of America may require a * specific license from the United States Government.  It is the * responsibility of any person or organization contemplating export to * obtain such a license before exporting. *  * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and distribute * this software and its documentation for any purpose and without fee is * hereby granted, provided that the above copyright notice appear in all * copies and that both that copyright notice and this permission notice * appear in supporting documentation, and that the name of M.I.T. not be * used in advertising or publicity pertaining to distribution of the * software without specific, written prior permission.  Furthermore if you * modify this software you must label your software as modified software * and not distribute it in such a fashion that it might be confused with * the original MIT software. M.I.T. makes no representations about the * suitability of this software for any purpose.  It is provided "as is" * without express or implied warranty. *  * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. *  * Individual source code files are copyright MIT, Cygnus Support, * OpenVision, Oracle, Sun Soft, FundsXpress, and others. *  * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira, * and Zephyr are trademarks of the Massachusetts Institute of Technology * (MIT).  No commercial use of these trademarks may be made without prior * written permission of MIT. *  * "Commercial use" means use of a name in a product or other for-profit * manner.  It does NOT prevent a commercial firm from referring to the MIT * trademarks in order to convey information (although in doing so, * recognition of their trademark status should be given). * $ *//* $Header: /cvs/repository/iservers/Libraries/passwordserver_sasl/cyrus_sasl/mac/CommonKClient/mac_kclient3/Headers/TicketKeeper/TicketKeeper.h,v 1.1 2002/02/28 00:28:37 snsimon Exp $ */#pragma once#include <Types.h>#include <Files.h>#include <Processes.h>#include <ConditionalMacros.h>#ifdef __cplusplusextern "C" {#endif#if PRAGMA_IMPORT#pragma import on#endif/* * TKAE_SendQuitApplication * * Send quit event to ticket keeper. This will cause Ticket Keeper to remove its * notification if it's up */OSStatusTKAE_SendQuitApplication ();/* * TKAE_SendOpenApplication * * Send open event to ticket keeper. Thiw will launch ticket keeper, and it will * display its self-dismissing notification if there's a problem */ OSStatusTKAE_SendOpenApplication ();/* * TKAE_SendOpenApplicationNoNotification * * Send open event to Ticket Keeper. This will launch Ticket Keeper, and it will not * display its notification if there's a problem. It will stay running. */ OSStatusTKAE_SendOpenApplicationNoNotification ();/* * TKAE_SendOpenApplicationNoNotificationFSSpec * * Send open event to the copy of Ticket Keeper specified in inTKFileSpec. * This will launch Ticket Keeper, and it will not * display its notification if there's a problem. It will stay running. */ OSStatusTKAE_SendOpenApplicationNoNotificationFSSpec (FSSpec *inTKFileSpec);/* * TKAE_SendGetStatus * * Get status from Ticket Keeper */OSStatusTKAE_SendGetStatus (	OSErr*	outStatus);	/* * IsTicketKeeperRunning * * Return true if TK is running, and fills out outPSN if the pointer is non-null. * Return false if TK is not running, and outPSN is unchanged. * */	BooleanIsTicketKeeperRunning (ProcessSerialNumber *outPSN);/*   FindTicketKeeperInExtensions()      Searches the startup volume for copies of Ticket Keeper and checks to see if   any of them are in the Extensions Folder.  If it finds one, returns true and   fills out *tkSpec.  If it doesn't find one or an error occurs, returns false   and *tkSpec is unchanged.      If the hard drive catalog changes during the search, continues anyway.      Uses functions from MoreFiles.*/BooleanFindTicketKeeperInExtensions(FSSpec *tkSpec);/*   TKAE_FindTargetTicketKeeper()      Searches the startup volume to find the Ticket Keeper that would receive   AppleEvents if any of the TicketKeeperLib functions that send AEs were called.   First checks to see if TK is running, and returns the FSSpec of that one if it is.   Next looks in the Extensions Folder.  Finally it searches the drive for   a copy.      If a Ticket Keeper is found, returns true and fills out *tkSpec.    If it doesn't find one or an error occurs, returns false and *tkSpec is unchanged.      If the hard drive catalog changes during the search, continues anyway.*/Boolean TKAE_FindTargetTicketKeeper(FSSpec *tkSpec);#if !TARGET_API_MAC_CARBON/*	Menu State functions		Ticket Keeper provides information needed for menus presented by the	Kerberos Control Strip and Kerberos Menu.  */struct MenuStateHeader;typedef struct MenuStateHeader MenuStateHeader;typedef MenuStateHeader**	MenuState;/*	TKMS_GetMenuState		Returns the current menu state. Dispose with TKMS_DisposeMenuState*/OSErr TKMS_GetMenuState (MenuState* outMenuState);/*	TKMS_DisposeMenuState		Disposes the menu state.*/void TKMS_DisposeMenuState (MenuState outMenuState);/*	TKMS_GetDefaultCacheExpiration		Pass in the menu state returned by TKMS_GetMenuState	Returns the expiration time of the default cache, in Mac epoch*/	OSErr TKMS_GetDefaultCacheExpiration (MenuState inState, UInt32* outExpiration);/*	TKMS_GetDefaultCacheLastChangeTime		Pass in the menu state returned by TKMS_GetMenuState	Returns the last change time of the default cache, in Mac epoch*/	OSErr TKMS_GetDefaultCacheLastChangeTime (MenuState inState, UInt32* outChangeTime);/*	TKMS_GetDefaultCachePrincipal		Pass in the menu state returned by TKMS_GetMenuState	Returns the principal of the default cache, realm removed if necessary*/	OSErr TKMS_GetDefaultCachePrincipal (MenuState inState, Str255 outPrincipal);/*	TKMS_GetDefaultCacheDisplayPrincipal		Pass in the menu state returned by TKMS_GetMenuState	Returns the principal of the default cache, quoting removed*/	OSErr TKMS_GetDefaultCacheDisplayPrincipal (MenuState inState, Str255 outPrincipal);/*	TKMS_GetDefaultCacheShortDisplayPrincipal		Pass in the menu state returned by TKMS_GetMenuState	Returns the principal of the default cache, quoting and default realm removed*/	OSErr TKMS_GetDefaultCacheShortDisplayPrincipal (MenuState inState, Str255 outPrincipal);/*	TKMS_GetDefaultCacheHasValidTickets		Pass in the menu state returned by TKMS_GetMenuState	Returns whether the default cache has valid tickets*/	OSErr TKMS_GetDefaultCacheHasValidTickets (MenuState inState, Boolean* outValidTickets);/*	TKMS_GetNumberOfCaches		Pass in the menu state returned by TKMS_GetMenuState	Returns the total number of caches in the list*/	OSErr TKMS_GetNumberOfCaches (MenuState inState, UInt32* outNumCaches);/*	TKMS_SortCachesAlphabetically		Pass in the menu state returned by TKMS_GetMenuState	Sorts the caches in the list alphabetically by principal*/	OSErr TKMS_SortCachesAlphabetically (	MenuState	inState);/*	TKMS_GetCacheListChangeTime		Pass in the menu state returned by TKMS_GetMenuState	Returns the last change time of the cache list in Mac epoch*/	OSErr TKMS_GetCacheListLastChangeTime (MenuState inState, UInt32* outChangeTime);/*	TKMS_GetIndexedCachePrincipal		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the cache principal of the cache at the index*/	OSErr TKMS_GetIndexedCachePrincipal (MenuState inState, UInt32 inIndex, Str255 outPrincipal);/*	TKMS_GetIndexedCacheDisplayPrincipal		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the cache principal of the cache at the index, quoting removed*/	OSErr TKMS_GetIndexedCacheDisplayPrincipal (MenuState inState, UInt32 inIndex, Str255 outPrincipal);/*	TKMS_GetIndexedCacheShortDisplayPrincipal		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the cache principal of the cache at the index, quoting and default realm removed*/	OSErr TKMS_GetIndexedCacheShortDisplayPrincipal (MenuState inState, UInt32 inIndex, Str255 outPrincipal);/*	TKMS_GetIndexedCacheVersion		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the cache version of the cache at the index (version constants same as ccahe and login lib*/	OSErr TKMS_GetIndexedCacheVersion (MenuState inState, UInt32 inIndex, UInt32* outVersion);/*	TKMS_GetIndexedCacheIsDefault		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns whether the cache at the index is default*/	OSErr TKMS_GetIndexedCacheIsDefault (MenuState inState, UInt32 inIndex, Boolean* outIsDefault);/*	TKMS_GetIndexedCacheIsValid		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns whether the cache at the index has valid tickets*/	OSErr TKMS_GetIndexedCacheIsValid (MenuState inState, UInt32 inIndex, Boolean* outIsValid);/*	TKMS_GetIndexedCacheStartTime		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the start time of the cache at the index in Mac epoch*/	OSErr TKMS_GetIndexedCacheStartTime (MenuState inState, UInt32 inIndex, UInt32* outStartTime);/*	TKMS_GetIndexedCacheExpirationTime		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Returns the expiration time of the cache at the index in Mac epoch*/	OSErr TKMS_GetIndexedCacheExpirationTime (MenuState inState, UInt32 inIndex, UInt32* outExpirationTime);/*	TKMS_SetIndexedDefaultCache		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Sets the cache at the index to be default*/	OSErr TKMS_SetIndexedDefaultCache (MenuState inState, UInt32 inIndex);/*	TKMS_DestroyIndexedCache		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Destroys the cache*/	OSErr TKMS_DestroyIndexedCache (MenuState inState, UInt32 inIndex);/*	TKMS_RenewIndexedCache		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Renews the cache*/	OSErr TKMS_RenewIndexedCache (MenuState inState, UInt32 inIndex);/*	TKMS_ChangeIndexedCachePassword		Pass in the menu state returned by TKMS_GetMenuState	Pass in index (zero based, less than value returned from TKMS_GetCachePrincipalForIndex)	Changes the password of the principal associated with the cache*/	OSErr TKMS_ChangeIndexedCachePassword (MenuState inState, UInt32 inIndex);/*	TKMS_GetFloaterStructureRegion		Pass in the menu state returned by TKMS_GetMenuState	Copies the structure region of the floater window into ioRegion. Used by tear-off MDEF.*/	OSErr TKMS_GetFloaterStructureRegion (	MenuState	inState,	RgnHandle	ioRegion);/*	TKMS_DestroyDefaultCache	Destroys the default cache	*/	OSErr TKMS_DestroyDefaultCache (void);/*	TKMS_NewLogin	Create a new cache, log in a new principal.*/	OSErr TKMS_NewLogin (void);/*	TKMS_RenewDefaultCache	Renew tickets for the default cache*/	OSErr TKMS_RenewDefaultCache (void);/*	TKMS_ChangeDefaultCachePassword	Changes the password of the principal associated with the default cache*/	OSErr TKMS_ChangeDefaultCachePassword (void);/*	TKMS_MoveFloaterStructureRegion		Pass in the new bounding box for the structure region of the floater	Moves the floater to the new location*/	OSErr TKMS_MoveFloaterStructureRegion (	const Rect*	inNewBounds);/*	TKMS_OpenKerberosControlPanel		Open the Kerberos Control Panel (if you can't calle KMLib because you're not	able to send events)*/	OSErr TKMS_OpenKerberosControlPanel (void);/*	TKF_SetHasCloseBox		Set whether the floater has a close box or not*/	OSErr TKF_SetHasCloseBox (	Boolean		inHasCloseBox);/*	TKF_GetHasCloseBox		return whether the floater has a close box or not*/	OSErr TKF_GetHasCloseBox (	Boolean*	outHasCloseBox);/*	TKF_SetDrawPies		Set whether the floater draws pies for time remaining or not*/	OSErr TKF_SetDrawPies (	Boolean		inDrawPies);/*	TKF_GetDrawPies		return whether the floater draws pies for time remaining or not*/	OSErr TKF_GetDrawPies (	Boolean*	outDrawPies);/*	TKF_SetIsVisible		Set whether the floater is visible or not*/	OSErr TKF_SetIsVisible (	Boolean		inIsVisible);/*	TKF_GetIsVisible		Return whether the floater is visible or not*/	OSErr TKF_GetIsVisible (	Boolean*	outIsVisible);/*	TKF_SetIsZoomedOut		Set whether the floater is zoomed out or not*/	OSErr TKF_SetIsZoomedOut (	Boolean		inIsZoomedOut);/*	TKF_GetIsZoomedOut		Return whether the floater is zoomed out or not*/	OSErr TKF_GetIsZoomedOut (	Boolean*	outIsZoomedOut);#endif /* !TARGET_API_MAC_CARBON */#ifdef PRAGMA_IMPORT_OFF#pragma import off#elif PRAGMA_IMPORT#pragma import reset#endif#ifdef __cplusplus}#endif/* * Constants */enum {	keyDontQuit				= FOUR_CHAR_CODE ('stay'),	keyStatus				= FOUR_CHAR_CODE ('Stat')};enum {	kTicketKeeperClass		= FOUR_CHAR_CODE ('TixK')};enum {	kAEGetStatus			= FOUR_CHAR_CODE ('Stat')};enum {	kTicketKeeperSignature	= FOUR_CHAR_CODE ('TixK')};