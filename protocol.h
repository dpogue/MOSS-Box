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
 * Various protocol values.
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

typedef uint32_t kinum_t;

/* 
 * 1: "Internal Error"
 * 2: "No Response From Server"
 * 3: "Invalid Server Data"
 * 4: "Age Not Found"
 * 5: "Unable to connect to Myst Online."
 * 6: "Disconnected from Myst Online."
 * 7: "File Not Found"
 * 8: "Old Build"
 * 9: "Remote Shutdown"
 * 10: "Database Timeout"
 * 11: "Account Already Exists"
 * 12: "Player Already Exists"
 * 13: "Account Not Found."
 * 14: "Player Not Found"
 * 15: "Invalid Parameter"
 * 16: "Name Lookup Failed"
 * 17: "Logged In Elsewhere"
 * 18: "Vault Node Not Found"
 * 19: "Max Players On Account"
 * 20: "Incorrect password.\nMake sure CAPS LOCK is not on."
 * 21: "State Object Not Found"
 * 22: "Login Denied"
 * 23: "Circular Reference"
 * 24: "Account Not Activated."
 * 25: "Key Already Used"
 * 26: "Key Not Found"
 * 27: "Activation Code Not Found"
 * 28: "Player Name Invalid"
 * 29: "Not Supported"
 * 30: "Service Forbidden"
 * 31: "Auth Token Too Old"
 * 32: "Must Use GameTap Client"
 * 33: "Too Many Failed Logins"
 * 34: "Unable to connect to GameTap, please try again in a few minutes."
 * 35: "GameTap: Too Many Auth Options"
 * 36: "GameTap: Missing Parameter"
 * 37: see 34
 * 38: "Your account has been banned from accessing Myst Online.  If you are unsure as to why this happened please contact customer support."
 * 39: "Account kicked by CCR"
 * 40: "Wrong score type for operation"
 * 41: "Not enough points"
 * 42: "Non-fixed score already exists"
 * 43: "No score data found"
 * 44: "Invite: Couldn't find player"
 * 45: "Invite: Too many hoods"
 * >= 46: "Unknown error"
 * -1: "Pending"
 */

typedef enum {
  NO_ERROR =                   0,
  ERROR_INTERNAL =             1,  ///< "Internal Error"
  ERROR_NO_RESPONSE =          2,  ///< "No Response From Server"
  ERROR_INVALID_DATA =         3,  ///< "Invalid Server Data"
  ERROR_AGE_NOT_FOUND =        4,  ///< "Age Not Found"
                                   //   5: "Unable to connect to Myst Online."
  ERROR_DISCONNECTED =         6,  ///< "Disconnected from Myst Online."
  ERROR_FILE_NOT_FOUND =       7,  ///< "File Not Found"
                                   //   8: "Old Build"
  ERROR_REMOTE_SHUTDOWN =      9,  ///< "Remote Shutdown"
  ERROR_DB_TIMEOUT =           10, ///< "Database Timeout"
                                   //   10: "Account Already Exists"
  ERROR_PLAYER_EXISTS =        12, ///< "Player Already Exists"
  ERROR_ACCT_NOT_FOUND =       13, ///< "Account Not Found."
  ERROR_PLAYER_NOT_FOUND =     14, ///< "Player Not Found"
  ERROR_INVALID_PARAM =        15, ///< "Invalid Parameter"
  ERROR_NAME_LOOKUP =          16, ///< "Name Lookup Failed"
  ERROR_LOGGED_IN_ELSEWHERE =  17, ///< "Logged In Elsewhere"
  ERROR_NODE_NOT_FOUND =       18, ///< "Vault Node Not Found"
  ERROR_MAX_PLAYERS =          19, ///< "Max Players On Account"
  ERROR_BAD_PASSWD =           20, ///< "Incorrect password.\nMake sure CAPS LOCK is not on."
                                   //   21: "State Object Not Found"
  ERROR_LOGIN_DENIED =         22, ///< "Login Denied"
                                   //   23: "Circular Reference"
                                   //   24: "Account Not Activated."
                                   //   25: "Key Already Used"
  ERROR_KEY_NOT_FOUND =        26, ///< "Key Not Found"
                                   //   27: "Activation Code Not Found"
  ERROR_NAME_INVALID =         28, ///< "Player Name Invalid"
  ERROR_NOT_SUPPORTED =        29, ///< "Not Supported"
  ERROR_FORBIDDEN =            30, ///< "Service Forbidden"
  ERROR_AUTH_TOO_OLD =         31, ///< "Auth Token Too Old"
                                   //   32: "Must Use GameTap Client"
  ERROR_TOO_MANY_FAILURES =    33, ///< "Too Many Failed Logins"
                                   //   34: "Unable to connect to GameTap, please try again in a few minutes."
                                   //   35: "GameTap: Too Many Auth Options"
                                   //   36: "GameTap: Missing Parameter"
                                   //   37: see 34
  ERROR_BANNED =               38, ///< "Your account has been banned from accessing Myst Online.  If you are unsure as to why this happened please contact customer support."
  ERROR_KICKED =               39, ///< "Account kicked by CCR"
  ERROR_BAD_SCORE_TYPE =       40, ///< "Wrong score type for operation"
  ERROR_SCORE_TOO_SMALL =      41, ///< "Not enough points"
  ERROR_SCORE_EXISTS =         42, ///< "Non-fixed score already exists"
  ERROR_NO_SCORE    =          43, ///< "No score data found"
                                   //   44: "Invite: Couldn't find player"
                                   //   45: "Invite: Too many hoods"
                                   //   >= 46: "Unknown error"
                                   //   -1: "Pending"
} status_code_t;

/*
 * Visitor/paying status (accounts and players)
 */
typedef enum {
  GUEST_CUSTOMER = 0x0,
  PAYING_CUSTOMER = 0x1
} customer_type_t;

/***************************************************************//*
 * @addtogroup Vault
 * @{
 * Vault bitfield values
 */

typedef enum vault_bitfield_e {
  NodeID =        0x00000001,  ///< 4 bytes
  CreateTime =    0x00000002,  ///< 4 bytes
  ModifyTime =    0x00000004,  ///< 4 bytes
  CreateAgeName = 0x00000008,  ///< 4-byte len + widestring
  CreateAgeUUID = 0x00000010,  ///< UUID (16 bytes)
  CreatorAcctID = 0x00000020,  ///< UUID (16 bytes)
  CreatorID =     0x00000040,  ///< 4 bytes
  NodeType =      0x00000080,  ///< 4 bytes
  Int32_1 =       0x00000100,  ///< 4 bytes
  Int32_2 =       0x00000200,  ///< 4 bytes
  Int32_3 =       0x00000400,  ///< 4 bytes
  Int32_4 =       0x00000800,  ///< 4 bytes
  UInt32_1 =      0x00001000,  ///< 4 bytes
  UInt32_2 =      0x00002000,  ///< 4 bytes
  UInt32_3 =      0x00004000,  ///< 4 bytes
  UInt32_4 =      0x00008000,  ///< 4 bytes
  UUID_1 =        0x00010000,  ///< UUID (16 bytes)
  UUID_2 =        0x00020000,  ///< UUID (16 bytes)
  UUID_3 =        0x00040000,  ///< UUID (16 bytes)
  UUID_4 =        0x00080000,  ///< UUID (16 bytes)
  String64_1 =    0x00100000,  ///< 4-byte len + widestring
  String64_2 =    0x00200000,  ///< 4-byte len + widestring
  String64_3 =    0x00400000,  ///< 4-byte len + widestring
  String64_4 =    0x00800000,  ///< 4-byte len + widestring
  String64_5 =    0x01000000,  ///< 4-byte len + widestring
  String64_6 =    0x02000000,  ///< 4-byte len + widestring
  IString64_1 =   0x04000000,  ///< 4-byte len + widestring
  IString64_2 =   0x08000000,  ///< 4-byte len + widestring
  Text_1 =        0x10000000,  ///< 4-byte len + widestring
  Text_2 =        0x20000000,  ///< 4-byte len + widestring
  Blob_1 =        0x40000000,  ///< 4-byte len + blob
  Blob_2 =        0x80000000   ///< 4-byte len + blob
// second bitfield unused??
} vault_bitfield_t;
/** @} */

/***************************************************************//*
 * plNetMsg flags
 * (c.f. CWE PubUtilLib/plNetMessage)
 */
enum BitVectorFlags // indicates what is present in the message, always transmitted
{
  HasTimeSent =            0x1,          ///< means fTimeSent need sending
  HasGameMsgRcvrs =        0x2,          ///< means that this is a direct (not bcast) game msg
  EchoBackToSender =       0x4,          ///< if broadcasting, echo packet back to sender
  RequestP2P =             0x8,          ///< sent to gameServer on joinReq
  AllowTimeOut =           0x10,         ///< sent to gameServer on joinReq (if release code)
  IndirectMember =         0x20,         ///< sent to client on joinAck if he is behind a firewall
  PublicIPClient =         0x40,         ///< set on a client coming from a public IP
                                          /// This flag is used when the servers are firewalled and NAT'ed
                                          /// It tells a game or lobby server to ask the lookup for an external address
  HasContext =             0x80,         ///< whether or not to write fContext field
  AskVaultForGameState =   0x100,        ///< Used with StartProcess server msgs. Whether or not the ServerAgent
                                          ///  must first ask the vault for a game state associated with the
                                          ///  game about to be instanced.
  HasTransactionID =       0x200,        ///< whether or not to write fTransactionID field
  NewSDLState =            0x400,        ///< set by client on first state packet sent, may not be necessary anymore
  InitialAgeStateRequest = 0x800,        ///< initial request for the age state
  HasPlayerID =            0x1000,       ///< is fPlayerID set
  UseRelevanceRegions =    0x2000,       ///< if players use relevance regions are used, this will be filtered by region, currently set on avatar physics and control msgs
  HasAcctUUID =            0x4000,       ///< is fAcctUUID set
  InterAgeRouting =        0x8000,       ///< give to pls for routing.
  HasVersion =             0x10000,      ///< if version is set
  IsSystemMessage =        0x20000,
  NeedsReliableSend =      0x40000,
  RouteToAllPlayers =      0x80000,      ///< send this message to all online players.
};

/*
 * no pl* type
 */
#define no_plType            0x8000

/***************************************************************//*
 * compression flags
 * (c.f. CWE PubUtilLib/plNetMessage)
 */
enum CompressionType   // currently only used for plNetMsgStreams
{
  CompressionNone,    ///< not compressed
  CompressionFailed,  ///< failed to compress
  CompressionZlib,    ///< zlib compressed
  CompressionDont     ///< don't compress
};


/***************************************************************//*
 * GameMgrMsg types
 */

enum {
  Srv2Cli_Game_PlayerJoined = 0,
  Srv2Cli_Game_PlayerLeft,
  Srv2Cli_Game_InviteFailed,
  Srv2Cli_Game_OwnerChange,
  Srv2Cli_NumGameMsgIds
};


// server->client

/*
 * cf. CWE NucleusLib/pnGameMgr/VarSync
 */
enum {
  Srv2Cli_VarSync_StringVarChanged = Srv2Cli_NumGameMsgIds,
  Srv2Cli_VarSync_NumericVarChanged,
  Srv2Cli_VarSync_AllVarsSent,
  Srv2Cli_VarSync_StringVarCreated,
  Srv2Cli_VarSync_NumericVarCreated
};

/*
 * c.f. CWE NucleusLib/pnGameMgr/BlueSpiral
 */
enum {
  Srv2Cli_BlueSpiral_ClothOrder = Srv2Cli_NumGameMsgIds,
  Srv2Cli_BlueSpiral_SuccessfulHit,
  Srv2Cli_BlueSpiral_GameWon,
  Srv2Cli_BlueSpiral_GameOver,      ///< sent on time out and incorrect entry
  Srv2Cli_BlueSpiral_GameStarted,
};

/*
 * cf. CWE NucleusLib/pnGameMgr/Marker
 */
enum {
  Srv2Cli_Marker_TemplateCreated = Srv2Cli_NumGameMsgIds,
  Srv2Cli_Marker_TeamAssigned,
  Srv2Cli_Marker_GameType,
  Srv2Cli_Marker_GameStarted,
  Srv2Cli_Marker_GamePaused,
  Srv2Cli_Marker_GameReset,
  Srv2Cli_Marker_GameOver,
  Srv2Cli_Marker_GameNameChanged,
  Srv2Cli_Marker_TimeLimitChanged,
  Srv2Cli_Marker_GameDeleted,
  Srv2Cli_Marker_MarkerAdded,
  Srv2Cli_Marker_MarkerDeleted,
  Srv2Cli_Marker_MarkerNameChanged,
  Srv2Cli_Marker_MarkerCaptured,
};

/*
 * cf. CWE NucleusLib/pnGameMgr/Heek
 */
enum {
  Srv2Cli_Heek_PlayGame = Srv2Cli_NumGameMsgIds, ///<  Sent when the server allows or disallows a player to play
  Srv2Cli_Heek_Goodbye,                           ///<  Sent when the server confirms the player leaving
  Srv2Cli_Heek_Welcome,                           ///<  Sent to everyone when a new player joins
  Srv2Cli_Heek_Drop,                              ///<  Sent when the admin needs to reset a position
  Srv2Cli_Heek_Setup,                             ///<  Sent on link-in so observers see the correct game state (fast-forwarded)
  Srv2Cli_Heek_LightState,                        ///<  Sent to a player when a light he owns changes state (animated)
  Srv2Cli_Heek_InterfaceState,                    ///<  Sent to a player when his buttons change state (animated)
  Srv2Cli_Heek_CountdownState,                    ///<  Sent to the admin to adjust the countdown state
  Srv2Cli_Heek_WinLose,                           ///<  Sent to a player when he wins or loses a hand
  Srv2Cli_Heek_GameWin,                           ///<  Sent to the admin when a game is won
  Srv2Cli_Heek_PointUpdate,                       ///<  Sent to a player when their points change
};

// constants

/*
 * cf. CWE NucleusLib/pnGameMgr
 */
enum EGameInviteError {
  GameInviteSuccess,
  GameInviteErrNotOwner,
  GameInviteErrAlreadyInvited,
  GameInviteErrAlreadyJoined,
  GameInviteErrGameStarted,
  GameInviteErrGameOver,
  GameInviteErrGameFull,
  GameInviteErrNoJoin,         ///< GameSrv reports the player may not join right now
  NumGameInviteErrors
};

/*
 * cf. CWE NucleusLib/pnGameMgr/Marker
 */
enum EMarkerGameType {
  MarkerGameQuest,
  MarkerGameCGZ,            ///< this is a quest game, but differentiating between the two on the client side makes some things easier
  MarkerGameCapture,
  MarkerGameCaptureAndHold,
  NumMarkerGameTypes
};

/* These are backwards from PlasmaConstants.py */
#define MarkerNotCaptured           0x00 /* name made up */
#define MarkerCaptured              0x01

/*
 * cf. CWE NucleusLib/pnGameMgr/Heek
 */
enum EHeekCountdownState {
  HeekCountdownStart,
  HeekCountdownStop,
  HeekCountdownIdle,
  NumHeekCountdownStates
};

enum EHeekChoice {
  HeekRock,
  HeekPaper,
  HeekScissors,
  NumHeekChoices
};

enum EHeekSeqFinished {
  HeekCountdownSeq,
  HeekChoiceAnimSeq,
  HeekGameWinAnimSeq,
  NumHeekSeq
};

enum EHeekLightState {
  HeekLightOn,
  HeekLightOff,
  HeekLightFlash,
  NumHeekLightStates
};

// client->server

/*
 * cf. CWE NucleusLib/pnGameMgr
 */
enum {
  Cli2Srv_Game_LeaveGame = 0,
  Cli2Srv_Game_Invite,
  Cli2Srv_Game_Uninvite,
  Cli2Srv_NumGameMsgIds
};

/*
 * cf. CWE NucleusLib/pnGameMgr/VarSync
 */
enum {
  Cli2Srv_VarSync_SetStringVar = Cli2Srv_NumGameMsgIds,
  Cli2Srv_VarSync_SetNumericVar,
  Cli2Srv_VarSync_RequestAllVars,
  Cli2Srv_VarSync_CreateStringVar,
  Cli2Srv_VarSync_CreateNumericVar,
};

/*
 * cf. CWE NucleusLib/pnGameMgr/BlueSpiral
 */
enum {
  Cli2Srv_BlueSpiral_StartGame = Cli2Srv_NumGameMsgIds,
  Cli2Srv_BlueSpiral_HitCloth,
};

/*
 *  * cf. CWE NucleusLib/pnGameMgr/Marker
 */
enum {
  Cli2Srv_Marker_StartGame = Cli2Srv_NumGameMsgIds,
  Cli2Srv_Marker_PauseGame,
  Cli2Srv_Marker_ResetGame,
  Cli2Srv_Marker_ChangeGameName,
  Cli2Srv_Marker_ChangeTimeLimit,
  Cli2Srv_Marker_DeleteGame,
  Cli2Srv_Marker_AddMarker,
  Cli2Srv_Marker_DeleteMarker,
  Cli2Srv_Marker_ChangeMarkerName,
  Cli2Srv_Marker_CaptureMarker,
};

/*
 * cf. CWE NucleusLib/pnGameMgr/Heek
 */
enum {
  Cli2Srv_Heek_PlayGame = Cli2Srv_NumGameMsgIds, ///< Sent when a player wants to join in the game (instead of observing)
  Cli2Srv_Heek_LeaveGame,                         ///< Sent when a player is done playing (and starts observing)
  Cli2Srv_Heek_Choose,                            ///< Sent when a player choses a move
  Cli2Srv_Heek_SeqFinished,                       ///< Sent when a client-side animation ends
};

#endif /* _PROTOCOL_H_ */
