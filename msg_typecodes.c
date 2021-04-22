/*
  MOSS - A server for the Myst Online: Uru Live client/protocol
  Copyright (C) 2008-2011  a'moaca'

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *
 * UruLive message typecodes string value functions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
"C" {
#endif

#include <stdint.h>
#include "msg_typecodes.h"

const char *Cli2File_e_c_str(int32_t t) {
  switch (t) {
    case Cli2File_ManifestRequest:               return "Cli2File_ManifestRequest";
    case Cli2File_FileDownloadRequest:           return "Cli2File_FileDownloadRequest";
    case Cli2File_FileDownloadChunkAck:          return "Cli2File_FileDownloadChunkAck";
    case Cli2File_PingRequest:                   return "Cli2File_PingRequest";
    case Cli2File_ManifestEntryAck:              return "Cli2File_ManifestEntryAck";
    case Cli2File_BuildIdRequest:                return "Cli2File_BuildIdRequest";
  }
  return "(unknown)";
}


const char *Cli2Auth_e_c_str(int32_t t) {
  switch (t) {
    case Cli2Auth_AcctActivateRequest:           return "Cli2Auth_AcctActivateRequest";
    case Cli2Auth_AcctChangePasswordRequest:     return "Cli2Auth_AcctChangePasswordRequest";
    case Cli2Auth_AcctCreateFromKeyRequest:      return "Cli2Auth_AcctCreateFromKeyRequest";
    case Cli2Auth_AcctCreateRequest:             return "Cli2Auth_AcctCreateRequest";
    case Cli2Auth_AcctLoginRequest:              return "Cli2Auth_AcctLoginRequest";
    case Cli2Auth_AcctSetBillingTypeRequest:     return "Cli2Auth_AcctSetBillingTypeRequest";
    case Cli2Auth_AcctSetPlayerRequest:          return "Cli2Auth_AcctSetPlayerRequest";
    case Cli2Auth_AcctSetRolesRequest:           return "Cli2Auth_AcctSetRolesRequest";
    case Cli2Auth_AgeRequest:                    return "Cli2Auth_AgeRequest";
    case Cli2Auth_ChangePlayerNameRequest:       return "Cli2Auth_ChangePlayerNameRequest";
    case Cli2Auth_ClientRegisterRequest:         return "Cli2Auth_ClientRegisterRequest";
    case Cli2Auth_ClientSetCCRLevel:             return "Cli2Auth_ClientSetCCRLevel";
    case Cli2Auth_FileDownloadChunkAck:          return "Cli2Auth_FileDownloadChunkAck";
    case Cli2Auth_FileDownloadRequest:           return "Cli2Auth_FileDownloadRequest";
    case Cli2Auth_FileListRequest:               return "Cli2Auth_FileListRequest";
    case Cli2Auth_GetPublicAgeList:              return "Cli2Auth_GetPublicAgeList";
    case Cli2Auth_KickPlayer:                    return "Cli2Auth_KickPlayer";
    case Cli2Auth_LogClientDebuggerConnect:      return "Cli2Auth_LogClientDebuggerConnect";
    case Cli2Auth_LogPythonTraceback:            return "Cli2Auth_LogPythonTraceback";
    case Cli2Auth_LogStackDump:                  return "Cli2Auth_LogStackDump";
    case Cli2Auth_PingRequest:                   return "Cli2Auth_PingRequest";
    case Cli2Auth_PlayerCreateRequest:           return "Cli2Auth_PlayerCreateRequest";
    case Cli2Auth_PlayerDeleteRequest:           return "Cli2Auth_PlayerDeleteRequest";
    case Cli2Auth_PropagateBuffer:               return "Cli2Auth_PropagateBuffer";
    case Cli2Auth_ScoreAddPoints:                return "Cli2Auth_ScoreAddPoints";
    case Cli2Auth_ScoreCreate:                   return "Cli2Auth_ScoreCreate";
    case Cli2Auth_ScoreDelete:                   return "Cli2Auth_ScoreDelete";
    case Cli2Auth_ScoreGetRanks:                 return "Cli2Auth_ScoreGetRanks";
    case Cli2Auth_ScoreGetScores:                return "Cli2Auth_ScoreGetScores";
    case Cli2Auth_ScoreSetPoints:                return "Cli2Auth_ScoreSetPoints";
    case Cli2Auth_ScoreTransferPoints:           return "Cli2Auth_ScoreTransferPoints";
    case Cli2Auth_SendFriendInviteRequest:       return "Cli2Auth_SendFriendInviteRequest";
    case Cli2Auth_SetAgePublic:                  return "Cli2Auth_SetAgePublic";
    case Cli2Auth_SetPlayerBanStatusRequest:     return "Cli2Auth_SetPlayerBanStatusRequest";
    case Cli2Auth_UpgradeVisitorRequest:         return "Cli2Auth_UpgradeVisitorRequest";
    case Cli2Auth_VaultFetchNodeRefs:            return "Cli2Auth_VaultFetchNodeRefs";
    case Cli2Auth_VaultInitAgeRequest:           return "Cli2Auth_VaultInitAgeRequest";
    case Cli2Auth_VaultNodeAdd:                  return "Cli2Auth_VaultNodeAdd";
    case Cli2Auth_VaultNodeCreate:               return "Cli2Auth_VaultNodeCreate";
    case Cli2Auth_VaultNodeFetch:                return "Cli2Auth_VaultNodeFetch";
    case Cli2Auth_VaultNodeFind:                 return "Cli2Auth_VaultNodeFind";
    case Cli2Auth_VaultNodeRemove:               return "Cli2Auth_VaultNodeRemove";
    case Cli2Auth_VaultNodeSave:                 return "Cli2Auth_VaultNodeSave";
    case Cli2Auth_VaultSendNode:                 return "Cli2Auth_VaultSendNode";
    case Cli2Auth_VaultSetSeen:                  return "Cli2Auth_VaultSetSeen";
  }
  return "(unknown)";
}

const char *Cli2Csr_e_c_str(int32_t t) {
  switch (t) {
    case Cli2Csr_LoginRequest:                   return "Cli2Csr_LoginRequest";
    case Cli2Csr_PingRequest:                    return "Cli2Csr_PingRequest";
    case Cli2Csr_RegisterRequest:                return "Cli2Csr_RegisterRequest";
  }
  return "(unknown)";
}

const char *Cli2Game_e_c_str(int32_t t) {
  switch (t) {
    case Cli2Game_GameMgrMsg:                    return "Cli2Game_GameMgrMsg";
    case Cli2Game_JoinAgeRequest:                return "Cli2Game_JoinAgeRequest";
    case Cli2Game_PingRequest:                   return "Cli2Game_PingRequest";
    case Cli2Game_PropagateBuffer:               return "Cli2Game_PropagateBuffer";
  }
  return "(unknown)";
}

const char *Cli2GateKeeper_e_c_str(int32_t t) {
  switch (t) {
    case Cli2GateKeeper_AuthSrvIpAddressRequest: return "Cli2GateKeeper_AuthSrvIpAddressRequest";
    case Cli2GateKeeper_FileSrvIpAddressRequest: return "Cli2GateKeeper_FileSrvIpAddressRequest";
    case Cli2GateKeeper_PingRequest:             return "Cli2GateKeeper_PingRequest";
  }
  return "(unknown)";
}

const char *Auth2Cli_e_c_str(int32_t t) {
  switch (t) {
    case Auth2Cli_AcctActivateReply:             return "Auth2Cli_AcctActivateReply";
    case Auth2Cli_AcctChangePasswordReply:       return "Auth2Cli_AcctChangePasswordReply";
    case Auth2Cli_AcctCreateFromKeyReply:        return "Auth2Cli_AcctCreateFromKeyReply";
    case Auth2Cli_AcctCreateReply:               return "Auth2Cli_AcctCreateReply";
    case Auth2Cli_AcctLoginReply:                return "Auth2Cli_AcctLoginReply";
    case Auth2Cli_AcctPlayerInfo:                return "Auth2Cli_AcctPlayerInfo";
    case Auth2Cli_AcctSetBillingTypeReply:       return "Auth2Cli_AcctSetBillingTypeReply";
    case Auth2Cli_AcctSetPlayerReply:            return "Auth2Cli_AcctSetPlayerReply";
    case Auth2Cli_AcctSetRolesReply:             return "Auth2Cli_AcctSetRolesReply";
    case Auth2Cli_AgeReply:                      return "Auth2Cli_AgeReply";
    case Auth2Cli_ChangePlayerNameReply:         return "Auth2Cli_ChangePlayerNameReply";
    case Auth2Cli_ClientRegisterReply:           return "Auth2Cli_ClientRegisterReply";
    case Auth2Cli_FileDownloadChunk:             return "Auth2Cli_FileDownloadChunk";
    case Auth2Cli_FileListReply:                 return "Auth2Cli_FileListReply";
    case Auth2Cli_KickedOff:                     return "Auth2Cli_KickedOff";
    case Auth2Cli_NotifyNewBuild:                return "Auth2Cli_NotifyNewBuild";
    case Auth2Cli_PingReply:                     return "Auth2Cli_PingReply";
    case Auth2Cli_PlayerCreateReply:             return "Auth2Cli_PlayerCreateReply";
    case Auth2Cli_PlayerDeleteReply:             return "Auth2Cli_PlayerDeleteReply";
    case Auth2Cli_PropagateBuffer:               return "Auth2Cli_PropagateBuffer";
    case Auth2Cli_PublicAgeList:                 return "Auth2Cli_PublicAgeList";
    case Auth2Cli_ScoreAddPointsReply:           return "Auth2Cli_ScoreAddPointsReply";
    case Auth2Cli_ScoreCreateReply:              return "Auth2Cli_ScoreCreateReply";
    case Auth2Cli_ScoreDeleteReply:              return "Auth2Cli_ScoreDeleteReply";
    case Auth2Cli_ScoreGetRanksReply:            return "Auth2Cli_ScoreGetRanksReply";
    case Auth2Cli_ScoreGetScoresReply:           return "Auth2Cli_ScoreGetScoresReply";
    case Auth2Cli_ScoreSetPointsReply:           return "Auth2Cli_ScoreSetPointsReply";
    case Auth2Cli_ScoreTransferPointsReply:      return "Auth2Cli_ScoreTransferPointsReply";
    case Auth2Cli_SendFriendInviteReply:         return "Auth2Cli_SendFriendInviteReply";
    case Auth2Cli_ServerAddr:                    return "Auth2Cli_ServerAddr";
    case Auth2Cli_SetPlayerBanStatusReply:       return "Auth2Cli_SetPlayerBanStatusReply";
    case Auth2Cli_UpgradeVisitorReply:           return "Auth2Cli_UpgradeVisitorReply";
    case Auth2Cli_VaultAddNodeReply:             return "Auth2Cli_VaultAddNodeReply";
    case Auth2Cli_VaultInitAgeReply:             return "Auth2Cli_VaultInitAgeReply";
    case Auth2Cli_VaultNodeAdded:                return "Auth2Cli_VaultNodeAdded";
    case Auth2Cli_VaultNodeChanged:              return "Auth2Cli_VaultNodeChanged";
    case Auth2Cli_VaultNodeCreated:              return "Auth2Cli_VaultNodeCreated";
    case Auth2Cli_VaultNodeDeleted:              return "Auth2Cli_VaultNodeDeleted";
    case Auth2Cli_VaultNodeFetched:              return "Auth2Cli_VaultNodeFetched";
    case Auth2Cli_VaultNodeFindReply:            return "Auth2Cli_VaultNodeFindReply";
    case Auth2Cli_VaultNodeRefsFetched:          return "Auth2Cli_VaultNodeRefsFetched";
    case Auth2Cli_VaultNodeRemoved:              return "Auth2Cli_VaultNodeRemoved";
    case Auth2Cli_VaultRemoveNodeReply:          return "Auth2Cli_VaultRemoveNodeReply";
    case Auth2Cli_VaultSaveNodeReply:            return "Auth2Cli_VaultSaveNodeReply";
  }
  return "(unknown)";
}

const char *Csr2Cli_e_c_str(int32_t t) {
  switch (t) {
    case Csr2Cli_LoginReply:                     return "Csr2Cli_LoginReply";
    case Csr2Cli_PingReply:                      return "Csr2Cli_PingReply";
    case Csr2Cli_RegisterReply:                  return "Csr2Cli_RegisterReply";
  }
  return "(unknown)";
}

const char *Game2Cli_e_c_str(int32_t t) {
  switch (t) {
    case Game2Cli_GameMgrMsg:                    return "Game2Cli_GameMgrMsg";
    case Game2Cli_JoinAgeReply:                  return "Game2Cli_JoinAgeReply";
    case Game2Cli_PingReply:                     return "Game2Cli_PingReply";
    case Game2Cli_PropagateBuffer:               return "Game2Cli_PropagateBuffer";
  }
  return "(unknown)";
}

const char *GateKeeper2Cli_e_c_str(int32_t t) {
  switch (t) {
    case GateKeeper2Cli_AuthSrvIpAddressReply:   return "GateKeeper2Cli_AuthSrvIpAddressReply";
    case GateKeeper2Cli_FileSrvIpAddressReply:   return "GateKeeper2Cli_FileSrvIpAddressReply";
    case GateKeeper2Cli_PingReply:               return "GateKeeper2Cli_PingReply";
  }
  return "(unknown)";
}


#ifdef __cplusplus
}
#endif
