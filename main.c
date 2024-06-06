#include "Settings.h"

#include <WITCH/WITCH.h>

#include <WITCH/PR/PR.h>

#include <WITCH/IO/IO.h>
#include <WITCH/IO/print.h>

#include <WITCH/VEC/VEC.h>
#include <WITCH/VEC/print.h>

void print(const char *format, ...){
  IO_fd_t fd_stdout;
  IO_fd_set(&fd_stdout, FD_OUT);
  va_list argv;
  va_start(argv, format);
  IO_vprint(&fd_stdout, format, argv);
  va_end(argv);
}

#include <WITCH/NET/TCP/TCP.h>
#include <WITCH/NET/TCP/TLS/TLS.h>

#include <WITCH/STR/psu.h>
#include <WITCH/STR/pss.h>
#include <WITCH/STR/psh.h>

#include <WITCH/FS/FS.h>

void SaveToFile(const char *FileName, const void *Data, uintptr_t DataSize){
  FS_file_t file;
  sint32_t err = FS_file_open(FileName, &file, O_CREAT | O_WRONLY);
  if(err != 0){
    print("SaveToFile failed with %lx\n", err);
    PR_abort();
  }

  for(uintptr_t i = 0; i != DataSize;){
    sintptr_t s = FS_file_write(&file, Data, DataSize);
    if(s < 0){
      print("FS_file_write err %x\n", s);
      PR_abort();
    }
    i += s;
  }

  FS_file_close(&file);
}
bool ReadFromFile(const char *FileName, void *Data, uintptr_t DataSize){
  FS_file_t file;
  sint32_t err = FS_file_open(FileName, &file, O_RDONLY);
  if(err != 0){
    print("SaveToFile failed with %lx\n", err);
    PR_abort();
  }

  sintptr_t s = FS_file_read(&file, Data, DataSize);

  FS_file_close(&file);

  if(s < 0){
    print("FS_file_read err %x\n", s);
    PR_abort();
  }
  if(s != DataSize){
    return false;
  }

  return true;
}

#define ETC_HTTP_set_prefix HTTP
#include <WITCH/ETC/HTTP/HTTP.h>

#define ETC_HTTP_Websock_set_prefix Websock
#include <WITCH/ETC/HTTP/Websock.h>

#define ETC_HTTP_pall_set_prefix hpall
#include <WITCH/ETC/HTTP/pall.h>

/* external json parser */
#include "jsmn.h"

/* for utf16 to utf8 */
#include "utf.h"

#define TotalCPType 2

typedef enum{
  CPType_Telegram,
  CPType_Discord
}CPType;

const char *CPShortName[TotalCPType] = {
  "tg",
  "dc"
};

#define set_NameSizeLimit 64

typedef struct{
  CPType CPFrom;
  uint32_t CPReferenceCount;
  sint64_t UserID;
  uint8_t Name[set_NameSizeLimit];
  uint32_t NameSize;
  void *Text;
  uintptr_t TextSize;
}Message_t;

#define BLL_set_prefix MessageList
#define BLL_set_NodeDataType Message_t
#define BLL_set_Language 0
#include <BLL/BLL.h>

MessageList_t MessageList;

struct{
  MessageList_NodeReference_t Verify;
  MessageList_NodeReference_t Sent;
}CPMessageAt[TotalCPType];

#define set_CP_Discord_SessionID_Size 32

typedef enum{
  CP_Telegram_Listen_PacketType_Unknown,
  CP_Telegram_Listen_PacketType_GetUpdate
}CP_Telegram_Listen_PacketType;

#define BLL_set_prefix CP_Telegram_Listen_PacketTypeList
#define BLL_set_NodeDataType CP_Telegram_Listen_PacketType
#define BLL_set_Language 0
#include <BLL/BLL.h>

typedef struct{
  hpall_t hpall; // open
  bool ContentTypeJSON; // false
  VEC_t jvec; // init

  CP_Telegram_Listen_PacketTypeList_t PacketTypeList;
}CP_Telegram_Listen_pd_t;

typedef enum{
  CP_Telegram_Send_PacketType_Unknown,
  CP_Telegram_Send_PacketType_GetMe, // used for keep alive
  CP_Telegram_Send_PacketType_SendMessage
}CP_Telegram_Send_PacketType;

#define BLL_set_prefix CP_Telegram_Send_PacketTypeList
#define BLL_set_NodeDataType CP_Telegram_Send_PacketType
#define BLL_set_Language 0
#include <BLL/BLL.h>

typedef struct{
  EV_timer_t KeepAliveTimer;

  hpall_t hpall; // open
  bool ContentTypeJSON; // false
  VEC_t jvec; // init

  CP_Telegram_Send_PacketTypeList_t PacketTypeList;
}CP_Telegram_Send_pd_t;

typedef enum{
  CP_Discord_Listen_PacketType_Unknown,
  CP_Discord_Listen_PacketType_10,
  CP_Discord_Listen_PacketType_11,
  CP_Discord_Listen_PacketType_2
}CP_Discord_Listen_PacketType;

#define BLL_set_prefix CP_Discord_Listen_PacketTypeList
#define BLL_set_NodeDataType CP_Discord_Listen_PacketType
#define BLL_set_Language 0
#include <BLL/BLL.h>

typedef struct{
  bool Upgraded; // false
  union{
    struct{
      HTTP_decode_t HTTP_decode; // init
      uint16_t HTTPCode;
    }BeforeUpgrade;
    struct{
      CP_Discord_Listen_PacketTypeList_t PacketTypeList; // CP_Discord_Listen_Init_AfterUpgrade
      Websock_Parser_t wparser; // CP_Discord_Listen_Init_AfterUpgrade
      VEC_t jvec; // CP_Discord_Listen_Init_AfterUpgrade
      uint64_t KeepAliveTimeMS; // will be assigned by receiving op 10
      bool KeepAliveStarted; // false, op 10
      EV_timer_t KeepAliveTimer; // when KeepAliveStarted set to true
    }AfterUpgrade;
  };
}CP_Discord_Listen_pd_t;

typedef enum{
  CP_Discord_Send_PacketType_Unknown,
  CP_Discord_Send_PacketType_GetCurrentUser, // used for keep alive
  CP_Discord_Send_PacketType_CreateMessage
}CP_Discord_Send_PacketType;

#define BLL_set_prefix CP_Discord_Send_PacketTypeList
#define BLL_set_NodeDataType CP_Discord_Send_PacketType
#define BLL_set_Language 0
#include <BLL/BLL.h>

typedef struct{
  EV_timer_t KeepAliveTimer;

  hpall_t hpall; // open
  bool ContentTypeJSON; // false
  VEC_t jvec; // init

  CP_Discord_Send_PacketTypeList_t PacketTypeList;
}CP_Discord_Send_pd_t;

typedef struct{
  EV_timer_t ConnectTimer;

  uint32_t ip;

  TLS_ctx_t tlsctx;

  NET_TCP_t *tcp;
  NET_TCP_peer_t *peer;

  NET_TCP_extid_t extid;
  NET_TCP_layerid_t LayerStateID;
  NET_TCP_layerid_t LayerReadID;
}tcppile_t;

typedef struct{
  EV_t listener;

  EV_timer_t InitialTimer;

  tcppile_t CP_Telegram_Listen;
  uint64_t CP_Telegram_UpdateID;
  tcppile_t CP_Telegram_Send;

  tcppile_t CP_Discord_Listen;
  tcppile_t CP_Discord_Send;
}pile_t;
pile_t pile;

void CP_Telegram_Listen_InsertPacketType(CP_Telegram_Listen_PacketType pt){
  CP_Telegram_Listen_pd_t *pd = (CP_Telegram_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Listen.peer, pile.CP_Telegram_Listen.extid);

  CP_Telegram_Listen_PacketTypeList_NodeReference_t nr = CP_Telegram_Listen_PacketTypeList_NewNodeLast(&pd->PacketTypeList);
  CP_Telegram_Listen_PacketTypeList_Node_t *n = CP_Telegram_Listen_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, nr);
  n->data = pt;
}

CP_Telegram_Listen_PacketType CP_Telegram_Listen_GetPacketType(){
  CP_Telegram_Listen_pd_t *pd = (CP_Telegram_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Listen.peer, pile.CP_Telegram_Listen.extid);

  if(CP_Telegram_Listen_PacketTypeList_Usage(&pd->PacketTypeList) == 0){
    return CP_Telegram_Listen_PacketType_Unknown;
  }

  CP_Telegram_Listen_PacketTypeList_NodeReference_t FirstPacketNR = CP_Telegram_Listen_PacketTypeList_GetNodeFirst(&pd->PacketTypeList);
  CP_Telegram_Listen_PacketTypeList_Node_t *n = CP_Telegram_Listen_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, FirstPacketNR);
  return n->data;
}

void CP_Telegram_Send_InsertPacketType(CP_Telegram_Send_PacketType pt){
  CP_Telegram_Send_pd_t *pd = (CP_Telegram_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Send.peer, pile.CP_Telegram_Send.extid);

  CP_Telegram_Send_PacketTypeList_NodeReference_t nr = CP_Telegram_Send_PacketTypeList_NewNodeLast(&pd->PacketTypeList);
  CP_Telegram_Send_PacketTypeList_Node_t *n = CP_Telegram_Send_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, nr);
  n->data = pt;
}

CP_Telegram_Send_PacketType CP_Telegram_Send_GetPacketType(){
  CP_Telegram_Send_pd_t *pd = (CP_Telegram_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Send.peer, pile.CP_Telegram_Send.extid);

  if(CP_Telegram_Send_PacketTypeList_Usage(&pd->PacketTypeList) == 0){
    return CP_Telegram_Send_PacketType_Unknown;
  }

  CP_Telegram_Send_PacketTypeList_NodeReference_t FirstPacketNR = CP_Telegram_Send_PacketTypeList_GetNodeFirst(&pd->PacketTypeList);
  CP_Telegram_Send_PacketTypeList_Node_t *n = CP_Telegram_Send_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, FirstPacketNR);
  return n->data;
}

void CP_Discord_Listen_InsertPacketType(CP_Discord_Listen_PacketType pt){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  CP_Discord_Listen_PacketTypeList_NodeReference_t nr = CP_Discord_Listen_PacketTypeList_NewNodeLast(&pd->AfterUpgrade.PacketTypeList);
  CP_Discord_Listen_PacketTypeList_Node_t *n = CP_Discord_Listen_PacketTypeList_GetNodeByReference(&pd->AfterUpgrade.PacketTypeList, nr);
  n->data = pt;
}
CP_Discord_Listen_PacketType CP_Discord_Listen_GetPacketType(){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  if(CP_Discord_Listen_PacketTypeList_Usage(&pd->AfterUpgrade.PacketTypeList) == 0){
    return CP_Discord_Listen_PacketType_Unknown;
  }

  CP_Discord_Listen_PacketTypeList_NodeReference_t FirstPacketNR = CP_Discord_Listen_PacketTypeList_GetNodeFirst(&pd->AfterUpgrade.PacketTypeList);
  CP_Discord_Listen_PacketTypeList_Node_t *n = CP_Discord_Listen_PacketTypeList_GetNodeByReference(&pd->AfterUpgrade.PacketTypeList, FirstPacketNR);
  return n->data;
}
void CP_Discord_Listen_RemovePacketType(){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  if(CP_Discord_Listen_PacketTypeList_Usage(&pd->AfterUpgrade.PacketTypeList) == 0){
    PR_abort();
  }

  CP_Discord_Listen_PacketTypeList_NodeReference_t FirstPacketNR = CP_Discord_Listen_PacketTypeList_GetNodeFirst(&pd->AfterUpgrade.PacketTypeList);
  CP_Discord_Listen_PacketTypeList_unlrec(&pd->AfterUpgrade.PacketTypeList, FirstPacketNR);
}

void CP_Discord_Send_InsertPacketType(CP_Discord_Send_PacketType pt){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  CP_Discord_Send_PacketTypeList_NodeReference_t nr = CP_Discord_Send_PacketTypeList_NewNodeLast(&pd->PacketTypeList);
  CP_Discord_Send_PacketTypeList_Node_t *n = CP_Discord_Send_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, nr);
  n->data = pt;
}

CP_Discord_Send_PacketType CP_Discord_Send_GetPacketType(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  if(CP_Discord_Send_PacketTypeList_Usage(&pd->PacketTypeList) == 0){
    return CP_Discord_Send_PacketType_Unknown;
  }

  CP_Discord_Send_PacketTypeList_NodeReference_t FirstPacketNR = CP_Discord_Send_PacketTypeList_GetNodeFirst(&pd->PacketTypeList);
  CP_Discord_Send_PacketTypeList_Node_t *n = CP_Discord_Send_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, FirstPacketNR);
  return n->data;
}

void VerifyMessage(CPType cp){
  MessageList_NodeReference_t *nr = &CPMessageAt[cp].Verify;
  MessageList_Node_t *n = MessageList_GetNodeByReference(&MessageList, *nr);
  MessageList_NodeReference_t nnr = n->NextNodeReference;
  if(--n->data.CPReferenceCount == 0){
    A_resize(n->data.Text, 0);
    MessageList_unlrec(&MessageList, *nr);
  }
  *nr = nnr;
}

void CP_Telegram_DeescapeString(const uint8_t *Input, uintptr_t InputSize, VEC_t *Output){
  bool EscapeCame = false;
  for(uintptr_t i = 0; i < InputSize;){
    if(EscapeCame == true){
      EscapeCame = false;
      if(Input[i] == '\\'){
        VEC_print(Output, "\\");
      }
      else if(Input[i] == 'n'){
        VEC_print(Output, "\n");
      }
      else if(Input[i] == 'u'){
        i++;
        if(InputSize - i < 4){
          return;
        }
        uint8_t hcode_size = 1;
        uint16_t hcode[2];
        hcode[0] = STR_psh32_digit(&Input[i], 4);
        i += 4;
        if(hcode[0] >= 0xd800 && hcode[0] <= 0xdbff){
          // need second now
          if(InputSize - i < 6){
            return;
          }
          if(Input[i++] != '\\'){
            return;
          }
          if(Input[i++] != 'u'){
            return;
          }
          hcode[1] = STR_psh32_digit(&Input[i], 4);
          if(hcode[1] >= 0xdc00 && hcode[1] <= 0xdfff); else{
            return;
          }
          i += 4;
          hcode_size++;
        }
        uint8_t utf8c[16];
        size_t utf8size = utf16_to_utf8(hcode, hcode_size, utf8c, 16);
        VEC_print(Output, "%.*s", utf8size, utf8c);
        continue;
      }
    }
    else if(Input[i] == '\\'){
      EscapeCame = true;
    }
    else{
      VEC_print(Output, "%c", Input[i]);
    }
    i++;
  }
}

void CP_Telegram_EscapeString(const uint8_t *Input, uintptr_t InputSize, VEC_t *Output){
  for(uintptr_t i = 0; i < InputSize; i++){
    if(Input[i] < 0x80){
      if(
        STR_ischar_digit(Input[i]) == false &&
        STR_ischar_char(Input[i]) == false
      ){
        VEC_print(Output, "%%%02lx", Input[i]);
      }
      else{
        VEC_print(Output, "%c", Input[i]);
      }
    }
    else{
      VEC_print(Output, "%c", Input[i]);
    }
  }
}

void CP_Telegram_Send_SendMessage(const Message_t *Message){
  CP_Telegram_Send_InsertPacketType(CP_Telegram_Send_PacketType_SendMessage);

  VEC_t buf;
  VEC_init(&buf, 1, A_resize);

  VEC_print(&buf, "GET /bot" set_CP_Telegram_Token "/sendMessage?chat_id=" set_CP_Telegram_GroupID "&text=");

  VEC_print(&buf, "%s:%llx ", CPShortName[Message->CPFrom], Message->UserID);
  VEC_print(&buf, "%.*s", Message->NameSize, Message->Name);
  VEC_print(&buf, "%%0a");

  CP_Telegram_EscapeString(Message->Text, Message->TextSize, &buf);

  VEC_print(&buf,
    " HTTP/1.1\r\n"
    "Host: api.telegram.org\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: en-US,en;q=0.5\r\n"
    "DNT: 1\r\n"
    "Connection: keep-alive\r\n\r\n");

  NET_TCP_Queue_t Queue;
  Queue.DynamicPointer.ptr = buf.ptr;
  Queue.DynamicPointer.size = buf.Current;
  NET_TCP_write_loop(
    pile.CP_Telegram_Send.peer,
    NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Telegram_Send.peer),
    NET_TCP_QueueType_DynamicPointer,
    &Queue);

  VEC_free(&buf);
}

void BroadcastMessages_Telegram(){
  MessageList_NodeReference_t *mnr = &CPMessageAt[CPType_Telegram].Sent;
  while(MessageList_inre(*mnr, MessageList.dst) == false){
    MessageList_Node_t *mn = MessageList_GetNodeByReference(&MessageList, *mnr);
    if(mn->data.CPFrom != CPType_Telegram){
      CP_Telegram_Send_SendMessage(&mn->data);
    }
    else{
      VerifyMessage(CPType_Telegram);
    }
    *mnr = mn->NextNodeReference;
  }
}

void CP_Discord_DeescapeString(const uint8_t *Input, uintptr_t InputSize, VEC_t *Output){
  bool EscapeCame = false;
  for(uintptr_t i = 0; i < InputSize; i++){
    if(EscapeCame == true){
      EscapeCame = false;
      if(Input[i] == '\\'){
        VEC_print(Output, "\\");
      }
      else if(Input[i] == 'n'){
        VEC_print(Output, "\n");
      }
    }
    else if(Input[i] < 0x80){
      if(Input[i] == '\\'){
        EscapeCame = true;
      }
      else{
        VEC_print(Output, "%c", Input[i]);
      }
    }
    else{
      VEC_print(Output, "%c", Input[i]);
    }
  }
}

void CP_Discord_EscapeString(const uint8_t *Input, uintptr_t InputSize, VEC_t *Output){
  for(uintptr_t i = 0; i < InputSize; i++){
    if(Input[i] < 0x80){
      if(Input[i] == '\\'){
        VEC_print(Output, "\\\\");
      }
      else if(Input[i] == '\n'){
        VEC_print(Output, "\\n");
      }
      else{
        VEC_print(Output, "%c", Input[i]);
      }
    }
    else{
      VEC_print(Output, "%c", Input[i]);
    }
  }
}

void CP_Discord_Send_SendMessage(const Message_t *Message){
  CP_Discord_Send_InsertPacketType(CP_Discord_Send_PacketType_CreateMessage);

  VEC_t buf;
  VEC_init(&buf, 1, A_resize);

  VEC_t jbuf;
  VEC_init(&jbuf, 1, A_resize);

  VEC_print(&jbuf,
    "{"
      "\"content\": \"");

  VEC_print(&jbuf, "```%s:%llx ", CPShortName[Message->CPFrom], Message->UserID);
  CP_Discord_EscapeString(Message->Name, Message->NameSize, &jbuf);
  VEC_print(&jbuf, "```");

  CP_Discord_EscapeString(Message->Text, Message->TextSize, &jbuf);

  VEC_print(&jbuf,
      "\","
      "\"tts\": false"
    "}");

  VEC_print(&buf,
    "POST /api/v9/channels/" set_CP_Discord_ChannelID "/messages HTTP/1.1\r\n"
    "Host: discord.com\r\n"
    "Authorization: Bot " set_CP_Discord_Token "\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: en-US,en;q=0.5\r\n"
    "Content-Type: application/json\r\n"
    "DNT: 1\r\n"
    "Connection: keep-alive\r\n"
    "Content-Length: %u\r\n\r\n"
    "%.*s",
    jbuf.Current, jbuf.Current, jbuf.ptr);

  VEC_free(&jbuf);

  NET_TCP_Queue_t Queue;
  Queue.DynamicPointer.ptr = buf.ptr;
  Queue.DynamicPointer.size = buf.Current;
  NET_TCP_write_loop(
    pile.CP_Discord_Send.peer,
    NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Discord_Send.peer),
    NET_TCP_QueueType_DynamicPointer,
    &Queue);

  VEC_free(&buf);
}

void BroadcastMessages_Discord(){
  MessageList_NodeReference_t *mnr = &CPMessageAt[CPType_Discord].Sent;
  while(MessageList_inre(*mnr, MessageList.dst) == false){
    MessageList_Node_t *mn = MessageList_GetNodeByReference(&MessageList, *mnr);
    if(mn->data.CPFrom != CPType_Discord){
      CP_Discord_Send_SendMessage(&mn->data);
    }
    else{
      VerifyMessage(CPType_Discord);
    }
    *mnr = mn->NextNodeReference;
  }
}

void BroadcastNewMessages(){
  if(pile.CP_Telegram_Send.peer != NULL){
    BroadcastMessages_Telegram();
  }
  if(pile.CP_Discord_Send.peer != NULL){
    BroadcastMessages_Discord();
  }
}

void Init_TCPPile(
  tcppile_t *tcppile,
  uint32_t ip,
  uintptr_t pd_size,
  NET_TCP_cb_state_t cb_connstate,
  NET_TCP_cb_read_t cb_read
){
  tcppile->ip = ip;

  tcppile->tcp = NET_TCP_alloc(&pile.listener);
  tcppile->peer = NULL;

  {
    bool r = TLS_ctx_generate(&tcppile->tlsctx);
    if(r != 0){
      PR_abort();
    }
    NET_TCP_TLS_add(tcppile->tcp, &tcppile->tlsctx);
  }

  tcppile->extid = NET_TCP_EXT_new(tcppile->tcp, 0, pd_size);
  tcppile->LayerStateID = NET_TCP_layer_state_open(
    tcppile->tcp,
    tcppile->extid,
    cb_connstate);
  tcppile->LayerReadID = NET_TCP_layer_read_open(
    tcppile->tcp,
    tcppile->extid,
    cb_read,
    0,
    0,
    0
  );

  NET_TCP_open(tcppile->tcp);
}

void ConnectServer_TCP_instant(tcppile_t *tcppile){
  NET_addr_t addr;
  addr.ip = tcppile->ip;
  addr.port = 443;

  NET_TCP_sockopt_t sockopt;
  sockopt.level = IPPROTO_TCP;
  sockopt.optname = TCP_NODELAY;
  sockopt.value = 1;

  NET_TCP_peer_t *FillerPeer;
  sint32_t err = NET_TCP_connect(tcppile->tcp, &FillerPeer, &addr, &sockopt, 1);
  if(err != 0){
    PR_abort();
  }
}
void _ConnectServer_TCP_cb(EV_t *listener, EV_timer_t *evt){
  tcppile_t *tcppile = OFFSETLESS(evt, tcppile_t, ConnectTimer);

  ConnectServer_TCP_instant(tcppile);

  EV_timer_stop(&pile.listener, &tcppile->ConnectTimer);
}
void ConnectServer_TCP(tcppile_t *tcppile){
  EV_timer_init(&tcppile->ConnectTimer, 4, _ConnectServer_TCP_cb);
  EV_timer_start(&pile.listener, &tcppile->ConnectTimer);
}

void CP_Telegram_Listen_HTTP_Open(){
  CP_Telegram_Listen_pd_t *pd = (CP_Telegram_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Listen.peer, pile.CP_Telegram_Listen.extid);

  hpall_Open(&pd->hpall);
  pd->ContentTypeJSON = false;
  VEC_init(&pd->jvec, 1, A_resize);
}
void CP_Telegram_Listen_HTTP_Reset(){
  CP_Telegram_Listen_pd_t *pd = (CP_Telegram_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Listen.peer, pile.CP_Telegram_Listen.extid);

  hpall_Reset(&pd->hpall);
  pd->ContentTypeJSON = false;
  pd->jvec.Current = 0;
}
void CP_Telegram_Listen_HTTP_Close(){
  CP_Telegram_Listen_pd_t *pd = (CP_Telegram_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Listen.peer, pile.CP_Telegram_Listen.extid);

  hpall_Close(&pd->hpall);
  VEC_free(&pd->jvec);
}

void CP_Telegram_Send_HTTP_Open(){
  CP_Telegram_Send_pd_t *pd = (CP_Telegram_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Send.peer, pile.CP_Telegram_Send.extid);

  hpall_Open(&pd->hpall);
  pd->ContentTypeJSON = false;
  VEC_init(&pd->jvec, 1, A_resize);
}
void CP_Telegram_Send_HTTP_Reset(){
  CP_Telegram_Send_pd_t *pd = (CP_Telegram_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Send.peer, pile.CP_Telegram_Send.extid);

  hpall_Reset(&pd->hpall);
  pd->ContentTypeJSON = false;
  pd->jvec.Current = 0;
}
void CP_Telegram_Send_HTTP_Close(){
  CP_Telegram_Send_pd_t *pd = (CP_Telegram_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Telegram_Send.peer, pile.CP_Telegram_Send.extid);

  hpall_Close(&pd->hpall);
  VEC_free(&pd->jvec);
}

void CP_Discord_Listen_Init_BeforeUpgrade(){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  HTTP_decode_init(&pd->BeforeUpgrade.HTTP_decode);
}
void CP_Discord_Listen_Reinit_AfterUpgrade(){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  Websock_Parser_init(&pd->AfterUpgrade.wparser);
  pd->AfterUpgrade.jvec.Current = 0;
}
void CP_Discord_Listen_Init_AfterUpgrade(){
  CP_Discord_Listen_pd_t *pd = (CP_Discord_Listen_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Listen.peer, pile.CP_Discord_Listen.extid);

  pd->Upgraded = true;
  CP_Discord_Listen_PacketTypeList_Open(&pd->AfterUpgrade.PacketTypeList);
  Websock_Parser_init(&pd->AfterUpgrade.wparser);
  VEC_init(&pd->AfterUpgrade.jvec, 1, A_resize);
  pd->AfterUpgrade.KeepAliveStarted = false;
}

void CP_Discord_Send_HTTP_Open(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  hpall_Open(&pd->hpall);
  pd->ContentTypeJSON = false;
  VEC_init(&pd->jvec, 1, A_resize);
}
void CP_Discord_Send_HTTP_Reset(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  hpall_Reset(&pd->hpall);
  pd->ContentTypeJSON = false;
  pd->jvec.Current = 0;
}
void CP_Discord_Send_HTTP_Close(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  hpall_Close(&pd->hpall);
  VEC_free(&pd->jvec);
}

void CP_Telegram_Listen_GetUpdate(){
  CP_Telegram_Listen_InsertPacketType(CP_Telegram_Listen_PacketType_GetUpdate);

  VEC_t buf;
  VEC_init(&buf, 1, A_resize);

  VEC_print(&buf,
    "GET /bot" set_CP_Telegram_Token "/getUpdates?timeout=8&limit=1&offset=%llu HTTP/1.1\r\n"
    "Host: api.telegram.org\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: en-US,en;q=0.5\r\n"
    "DNT: 1\r\n"
    "Connection: keep-alive\r\n\r\n",
  pile.CP_Telegram_UpdateID + 1);

  NET_TCP_Queue_t Queue;
  Queue.DynamicPointer.ptr = buf.ptr;
  Queue.DynamicPointer.size = buf.Current;
  NET_TCP_write_loop(
    pile.CP_Telegram_Listen.peer,
    NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Telegram_Listen.peer),
    NET_TCP_QueueType_DynamicPointer,
    &Queue);

  VEC_free(&buf);
}

#include "CP/Telegram/Listen/tcp_cb.h"
#include "CP/Telegram/Send/tcp_cb.h"
#include "CP/Discord/Listen/tcp_cb.h"
#include "CP/Discord/Send/tcp_cb.h"

void cb_InitialTimer(EV_t *listener, EV_timer_t *evt){
  ConnectServer_TCP_instant(&pile.CP_Telegram_Listen);
  ConnectServer_TCP_instant(&pile.CP_Telegram_Send);
  ConnectServer_TCP_instant(&pile.CP_Discord_Listen);
  ConnectServer_TCP_instant(&pile.CP_Discord_Send);

  EV_timer_stop(listener, evt);
}

int main(){
  MessageList_Open(&MessageList); // TOOD close it
  for(uint32_t i = 0; i < TotalCPType; i++){
    CPMessageAt[i].Verify = MessageList.dst;
    CPMessageAt[i].Sent = MessageList.dst;
  }

  pile.CP_Telegram_UpdateID = 0;

  EV_open(&pile.listener);

  Init_TCPPile(
    &pile.CP_Telegram_Listen,
    set_CP_Telegram_IP_Listen,
    sizeof(CP_Telegram_Listen_pd_t),
    (NET_TCP_cb_state_t)CP_Telegram_Listen_connstate,
    (NET_TCP_cb_read_t)CP_Telegram_Listen_read);
  Init_TCPPile(
    &pile.CP_Telegram_Send,
    set_CP_Telegram_IP_Send,
    sizeof(CP_Telegram_Send_pd_t),
    (NET_TCP_cb_state_t)CP_Telegram_Send_connstate,
    (NET_TCP_cb_read_t)CP_Telegram_Send_read);
  Init_TCPPile(
    &pile.CP_Discord_Listen,
    set_CP_Discord_IP_Listen,
    sizeof(CP_Discord_Listen_pd_t),
    (NET_TCP_cb_state_t)CP_Discord_Listen_connstate,
    (NET_TCP_cb_read_t)CP_Discord_Listen_read);
  Init_TCPPile(
    &pile.CP_Discord_Send,
    set_CP_Discord_IP_Send,
    sizeof(CP_Discord_Send_pd_t),
    (NET_TCP_cb_state_t)CP_Discord_Send_connstate,
    (NET_TCP_cb_read_t)CP_Discord_Send_read);

  EV_timer_init(&pile.InitialTimer, 1, cb_InitialTimer);
  EV_timer_start(&pile.listener, &pile.InitialTimer);

  EV_start(&pile.listener);

  return 0;
}
