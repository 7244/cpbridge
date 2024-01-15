uint32_t
CP_Telegram_Listen_connstate(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Telegram_Listen_pd_t *pd,
  uint32_t flag
){
  if(flag & NET_TCP_state_succ_e){
    print("[+] CP_Telegram_Listen\n");
    pile.CP_Telegram_Listen.peer = peer;

    CP_Telegram_Listen_HTTP_Open();

    CP_Telegram_Listen_PacketTypeList_Open(&pd->PacketTypeList);

    NET_TCP_StartReadLayer(peer, pile.CP_Telegram_Listen.LayerReadID);

    BroadcastMessages_Telegram();

    CP_Telegram_Listen_GetUpdate();
  }
  else do{
    ConnectServer_TCP(&pile.CP_Telegram_Listen);

    if(!(flag & NET_TCP_state_init_e)){
      break;
    }
    print("[-] CP_Telegram_Listen %lx\n", CP_Telegram_Listen_PacketTypeList_Usage(&pd->PacketTypeList));

    CP_Telegram_Listen_PacketTypeList_Close(&pd->PacketTypeList);

    CP_Telegram_Listen_HTTP_Close();

    pile.CP_Telegram_Listen.peer = NULL;
  }while(0);

  return 0;
}
NET_TCP_layerflag_t
CP_Telegram_Listen_read(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Telegram_Listen_pd_t *pd,
  NET_TCP_QueuerReference_t QueuerReference,
  uint32_t *type,
  NET_TCP_Queue_t *Queue
){
  uint8_t *ReadData;
  uintptr_t ReadSize;
  uint8_t _EventReadBuffer[0x1000];
  switch(*type){
    case NET_TCP_QueueType_DynamicPointer:{
      ReadData = (uint8_t *)Queue->DynamicPointer.ptr;
      ReadSize = Queue->DynamicPointer.size;
      break;
    }
    case NET_TCP_QueueType_PeerEvent:{
      IO_fd_t peer_fd;
      EV_event_get_fd(&peer->event, &peer_fd);
      IO_ssize_t len = IO_read(&peer_fd, _EventReadBuffer, sizeof(_EventReadBuffer));
      if(len < 0){
        NET_TCP_CloseHard(peer);
        return NET_TCP_EXT_PeerIsClosed_e;
      }
      ReadData = _EventReadBuffer;
      ReadSize = len;
      break;
    }
    case NET_TCP_QueueType_CloseHard:{
      return 0;
    }
    default:{
      print("cb_read *type %lx\r\n", *type);
      __abort();
      __unreachable(); /* TOOD compiler is dumb */
    }
  }

  uintptr_t ReadDataIndex = 0;

  while(1){
    hpall_ParsedData ParsedData;
    hpall_ParseType ParseType = hpall_Parse(&pd->hpall, ReadData, ReadSize, &ReadDataIndex, &ParsedData);
    if(ParseType == hpall_ParseType_Done){
      jsmn_parser jparser;
      jsmn_init(&jparser);
      jsmntok_t jtokens[256]; // TODO hardcode 256
      int jerr = jsmn_parse(&jparser, (const char *)pd->jvec.ptr, pd->jvec.Current, jtokens, 256); // TODO hardcode 256
      if(jerr < 0){
        print("CP_Telegram_Listen json parse failed %u:\n%.*s\n", pd->jvec.Current, pd->jvec.Current, pd->jvec.ptr);
        NET_TCP_CloseHard(peer);
        return NET_TCP_EXT_PeerIsClosed_e;
      }

      const uint32_t DepthTokenLimit = 32;
      struct{
        uint32_t Size;
        uint32_t Type;
        jsmntype_t jType;
      }DepthToken[DepthTokenLimit];
      uint32_t CurrentDepth = 0;

      if(CP_Telegram_Listen_PacketTypeList_Usage(&pd->PacketTypeList) == 0){
        PR_abort();
      }
      CP_Telegram_Listen_PacketType PacketType;
      CP_Telegram_Listen_PacketTypeList_NodeReference_t FirstPacketNR = CP_Telegram_Listen_PacketTypeList_GetNodeFirst(&pd->PacketTypeList);
      {
        CP_Telegram_Listen_PacketTypeList_Node_t *n = CP_Telegram_Listen_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, FirstPacketNR);
        PacketType = n->data;
      }

      if(PacketType == CP_Telegram_Listen_PacketType_GetUpdate){
        #include "Read_GetUpdate.h"
      }
      else{
        PR_abort();
      }

      CP_Telegram_Listen_PacketTypeList_unlrec(&pd->PacketTypeList, FirstPacketNR);

      CP_Telegram_Listen_HTTP_Reset();
    }
    else if(ParseType == hpall_ParseType_Error){
      PR_abort();
    }
    else if(ParseType == hpall_ParseType_NotEnoughData){
      break;
    }
    else if(ParseType == hpall_ParseType_HTTPHead){

    }
    else if(ParseType == hpall_ParseType_HTTPHeader){
      if(
        MEM_cstreu("Content-Type") == ParsedData.HTTP.header.s[0] &&
        STR_ncmp("Content-Type", ParsedData.HTTP.header.v[0], ParsedData.HTTP.header.s[0]) == false
      ){
        if(
          MEM_cstreu("application/json") == ParsedData.HTTP.header.s[1] &&
          STR_ncmp("application/json", ParsedData.HTTP.header.v[1], ParsedData.HTTP.header.s[1]) == false
        ){
          pd->ContentTypeJSON = true;
        }
      }
    }
    else if(ParseType == hpall_ParseType_HTTPDone){
      if(pd->ContentTypeJSON == false){
        /* we got something else than json */
        PR_abort();
      }
    }
    else if(ParseType == hpall_ParseType_Payload){
      VEC_print(&pd->jvec, "%.*s", ParsedData.Payload.Size, ParsedData.Payload.Data);
    }
    else{
      /* internal error, should be not reachable */
      PR_abort();
    }
  }

  return 0;
}
