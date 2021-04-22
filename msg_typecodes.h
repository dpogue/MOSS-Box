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
 * UruLive message typecodes
 *
 */

#ifndef _MSG_TYPECODES_H_
#define _MSG_TYPECODES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Enumarations for base network communications protocols */

extern const char* Cli2File_e_c_str(int32_t t);

// NetProtocolCli2File messages
enum NetProtocolCli2File_e {
  // Global
  Cli2File_PingRequest          = 0,

  // File server-related
  Cli2File_BuildIdRequest       = 10,
  // 11 through 19 skipped

  // Cache-related
  Cli2File_ManifestRequest      = 20,
  Cli2File_FileDownloadRequest  = 21,
  Cli2File_ManifestEntryAck     = 22,
  Cli2File_FileDownloadChunkAck = 23,
  // 24 through 29 skipped

  Cli2File_UNUSED_1             = 30,
};

enum NetProtocolFile2Cli_e {
  // Global
  File2Cli_PingReply            = 0,

  // File server-related
  File2Cli_BuildIdReply         = 10,
  File2Cli_BuildIdUpdate        = 11,
  // 12 through 19 skipped

  // Cache-related
  File2Cli_ManifestReply        = 20,
  File2Cli_FileDownloadReply    = 21,
  // 22 through 29 skipped

  File2Cli_UNUSED_1             = 30,
};

extern const char* Cli2Auth_e_c_str(int32_t t);
extern const char* Cli2Csr_e_c_str(int32_t t);
extern const char* Cli2Game_e_c_str(int32_t t);
extern const char* Cli2GateKeeper_e_c_str(int32_t t);
extern const char* Auth2Cli_e_c_str(int32_t t);
extern const char* Csr2Cli_e_c_str(int32_t t);
extern const char* Game2Cli_e_c_str(int32_t t);
extern const char* GateKeeper2Cli_e_c_str(int32_t t);

// NetProtocolCli2Auth messages (from CWE pnNpCli2Auth.h)
enum NetProtocolCli2Auth_e {
  // Global
  Cli2Auth_PingRequest,               ///< 0x00 keep-alive to the Auth server

  // Client
  Cli2Auth_ClientRegisterRequest,     ///< 0x01
  Cli2Auth_ClientSetCCRLevel,         ///< 0x02

  // Account
  Cli2Auth_AcctLoginRequest,          ///< 0x03
  Cli2Auth_AcctSetEulaVersion,        ///< 0x04
  Cli2Auth_AcctSetDataRequest,        ///< 0x05
  Cli2Auth_AcctSetPlayerRequest,      ///< 0x06
  Cli2Auth_AcctCreateRequest,         ///< 0x07
  Cli2Auth_AcctChangePasswordRequest, ///< 0x08
  Cli2Auth_AcctSetRolesRequest,       ///< 0x09
  Cli2Auth_AcctSetBillingTypeRequest, ///< 0x0a
  Cli2Auth_AcctActivateRequest,       ///< 0x0b
  Cli2Auth_AcctCreateFromKeyRequest,  ///< 0x0c

  // Player
  Cli2Auth_PlayerDeleteRequest,       ///< 0x0d
  Cli2Auth_PlayerUndeleteRequest,     ///< 0x0e
  Cli2Auth_PlayerSelectRequest,       ///< 0x0f
  Cli2Auth_PlayerRenameRequest,       ///< 0x10
  Cli2Auth_PlayerCreateRequest,       ///< 0x11
  Cli2Auth_PlayerSetStatus,           ///< 0x12
  Cli2Auth_PlayerChat,                ///< 0x13
  Cli2Auth_UpgradeVisitorRequest,     ///< 0x14
  Cli2Auth_SetPlayerBanStatusRequest, ///< 0x15
  Cli2Auth_KickPlayer,                ///< 0x16
  Cli2Auth_ChangePlayerNameRequest,   ///< 0x17
  Cli2Auth_SendFriendInviteRequest,   ///< 0x18

  // Vault
  Cli2Auth_VaultNodeCreate,           ///< 0x19
  Cli2Auth_VaultNodeFetch,            ///< 0x1a
  Cli2Auth_VaultNodeSave,             ///< 0x1b
  Cli2Auth_VaultNodeDelete,           ///< 0x1c
  Cli2Auth_VaultNodeAdd,              ///< 0x1d
  Cli2Auth_VaultNodeRemove,           ///< 0x1e
  Cli2Auth_VaultFetchNodeRefs,        ///< 0x1f
  Cli2Auth_VaultInitAgeRequest,       ///< 0x20
  Cli2Auth_VaultNodeFind,             ///< 0x21
  Cli2Auth_VaultSetSeen,              ///< 0x22
  Cli2Auth_VaultSendNode,             ///< 0x23

  // Ages
  Cli2Auth_AgeRequest,                ///< 0x24

  // File-related
  Cli2Auth_FileListRequest,           ///< 0x25
  Cli2Auth_FileDownloadRequest,       ///< 0x26
  Cli2Auth_FileDownloadChunkAck,      ///< 0x27

  // Game
  Cli2Auth_PropagateBuffer,           ///< 0x28


  // Public ages
  Cli2Auth_GetPublicAgeList,          ///< 0x29
  Cli2Auth_SetAgePublic,              ///< 0x2a

  // Log Messages
  Cli2Auth_LogPythonTraceback,        ///< 0x2b
  Cli2Auth_LogStackDump,              ///< 0x2c
  Cli2Auth_LogClientDebuggerConnect,  ///< 0x2d

  // Score
  Cli2Auth_ScoreCreate,               ///< 0x2e
  Cli2Auth_ScoreDelete,               ///< 0x2f
  Cli2Auth_ScoreGetScores,            ///< 0x30
  Cli2Auth_ScoreAddPoints,            ///< 0x31
  Cli2Auth_ScoreTransferPoints,       ///< 0x32
  Cli2Auth_ScoreSetPoints,            ///< 0x33
  Cli2Auth_ScoreGetRanks,             ///< 0x34

  Cli2Auth_AccountExistsRequest,      ///< 0x35

  NumCli2AuthMessages                 ///< 0x36
};
enum NetProtocolAuth2Cli_e {
  // Global
  Auth2Cli_PingReply,
  Auth2Cli_ServerAddr,
  Auth2Cli_NotifyNewBuild,

  // Client
  Auth2Cli_ClientRegisterReply,

  // Account
  Auth2Cli_AcctLoginReply,
  Auth2Cli_AcctData,
  Auth2Cli_AcctPlayerInfo,
  Auth2Cli_AcctSetPlayerReply,
  Auth2Cli_AcctCreateReply,
  Auth2Cli_AcctChangePasswordReply,
  Auth2Cli_AcctSetRolesReply,
  Auth2Cli_AcctSetBillingTypeReply,
  Auth2Cli_AcctActivateReply,
  Auth2Cli_AcctCreateFromKeyReply,

  // Player
  Auth2Cli_PlayerList,
  Auth2Cli_PlayerChat,
  Auth2Cli_PlayerCreateReply,
  Auth2Cli_PlayerDeleteReply,
  Auth2Cli_UpgradeVisitorReply,
  Auth2Cli_SetPlayerBanStatusReply,
  Auth2Cli_ChangePlayerNameReply,
  Auth2Cli_SendFriendInviteReply,

  // Friends
  Auth2Cli_FriendNotify,

  // Vault
  Auth2Cli_VaultNodeCreated,
  Auth2Cli_VaultNodeFetched,
  Auth2Cli_VaultNodeChanged,
  Auth2Cli_VaultNodeDeleted,
  Auth2Cli_VaultNodeAdded,
  Auth2Cli_VaultNodeRemoved,
  Auth2Cli_VaultNodeRefsFetched,
  Auth2Cli_VaultInitAgeReply,
  Auth2Cli_VaultNodeFindReply,
  Auth2Cli_VaultSaveNodeReply,
  Auth2Cli_VaultAddNodeReply,
  Auth2Cli_VaultRemoveNodeReply,

  // Ages
  Auth2Cli_AgeReply,

  // File-related
  Auth2Cli_FileListReply,
  Auth2Cli_FileDownloadChunk,

  // Game
  Auth2Cli_PropagateBuffer,

  // Admin
  Auth2Cli_KickedOff,

  // Public ages
  Auth2Cli_PublicAgeList,

  // Score
  Auth2Cli_ScoreCreateReply,
  Auth2Cli_ScoreDeleteReply,
  Auth2Cli_ScoreGetScoresReply,
  Auth2Cli_ScoreAddPointsReply,
  Auth2Cli_ScoreTransferPointsReply,
  Auth2Cli_ScoreSetPointsReply,
  Auth2Cli_ScoreGetRanksReply,

  Auth2Cli_AccountExistsReply,

  NumAuth2CliMessages
};

// NetProtocolCli2Game messages
enum NetProtocolCli2Game_e {
  // Global
  Cli2Game_PingRequest,

  // Age
  Cli2Game_JoinAgeRequest,

  // Game
  Cli2Game_PropagateBuffer,
  Cli2Game_GameMgrMsg,

  NumCli2GameMessages
};

enum NetProtocolGame2Cli_e {
  // Global
  Game2Cli_PingReply,

  // Age
  Game2Cli_JoinAgeReply,

  // Game
  Game2Cli_PropagateBuffer,
  Game2Cli_GameMgrMsg,

  NumGame2CliMessages
};

// NetProtocolCli2GateKeeper messages (must be <= (word)-1)
enum NetProtocolCli2GateKeeper_e {
  // Global
  Cli2GateKeeper_PingRequest,
  Cli2GateKeeper_FileSrvIpAddressRequest,
  Cli2GateKeeper_AuthSrvIpAddressRequest,

  NumCli2GateKeeperMessages
};

enum NetProtocolGateKeeper2Cli_e {
  // Global
  GateKeeper2Cli_PingReply,
  GateKeeper2Cli_FileSrvIpAddressReply,
  GateKeeper2Cli_AuthSrvIpAddressReply,

  NumGateKeeper2CliMessages
};

// Cli2Csr
enum NetProtocolCli2Csr_e {
  Cli2Csr_PingRequest     = 0, ///< Misc
  Cli2Csr_RegisterRequest = 1, ///< Encrypt
  Cli2Csr_LoginRequest    = 2, ///< Login
  Cli2Csr_PatchRequest    = 3, ///< Patch

  NumCli2CsrMessages
};

// Csr2Cli
enum NetProtocolCsr2Cli_e {
  Csr2Cli_PingReply       = 0, ///< Misc
  Csr2Cli_RegisterReply   = 1, ///< Encrypt
  Csr2Cli_LoginReply      = 2, ///< Login
  Cli2Csr_PatchReply      = 3, ///< Patch

  NumCsr2CliMessages
};

#endif /* _MSG_TYPECODES_H_ */

#ifdef __cplusplus
}
#endif
