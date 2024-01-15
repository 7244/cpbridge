void CP_Discord_Send_KeepAliveTimer_cb(EV_t *listener, EV_timer_t *evt, uint32_t flag){
  CP_Discord_Send_InsertPacketType(CP_Discord_Send_PacketType_GetCurrentUser);

  VEC_t buf;
  VEC_init(&buf, 1, A_resize);

  VEC_print(&buf,
    "GET /api/v9/users/@me HTTP/1.1\r\n"
    "Host: discord.com\r\n"
    "Authorization: Bot " set_CP_Discord_Token "\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: en-US,en;q=0.5\r\n"
    "Content-Type: application/json\r\n"
    "DNT: 1\r\n"
    "Connection: keep-alive\r\n\r\n"
  );

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

void CP_Discord_Send_KeepAliveTimer_Open(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  EV_timer_init(&pd->KeepAliveTimer, 8, (EV_timer_cb_t)CP_Discord_Send_KeepAliveTimer_cb);
  EV_timer_start(&pile.listener, &pd->KeepAliveTimer);
}
void CP_Discord_Send_KeepAliveTimer_Close(){
  CP_Discord_Send_pd_t *pd = (CP_Discord_Send_pd_t *)NET_TCP_GetPeerData(pile.CP_Discord_Send.peer, pile.CP_Discord_Send.extid);

  EV_timer_stop(&pile.listener, &pd->KeepAliveTimer);
}
void CP_Discord_Send_KeepAliveTimer_Update(){
  CP_Discord_Send_KeepAliveTimer_Close();
  CP_Discord_Send_KeepAliveTimer_Open();
}

uint32_t
CP_Discord_Send_connstate(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Discord_Send_pd_t *pd,
  uint32_t flag
){
  if(flag & NET_TCP_state_succ_e){
    print("[+] CP_Discord_Send\n");
    pile.CP_Discord_Send.peer = peer;

    CP_Discord_Send_HTTP_Open();

    CP_Discord_Send_PacketTypeList_Open(&pd->PacketTypeList);

    NET_TCP_StartReadLayer(peer, pile.CP_Discord_Send.LayerReadID);

    BroadcastMessages_Discord();

    CP_Discord_Send_KeepAliveTimer_Open();
  }
  else do{
    ConnectServer_TCP(&pile.CP_Discord_Send);

    if(!(flag & NET_TCP_state_init_e)){
      break;
    }
    print("[-] CP_Discord_Send %lx\n", CP_Discord_Send_PacketTypeList_Usage(&pd->PacketTypeList));

    CP_Discord_Send_KeepAliveTimer_Close();

    CPMessageAt[CPType_Discord].Sent = CPMessageAt[CPType_Discord].Verify;

    CP_Discord_Send_PacketTypeList_Close(&pd->PacketTypeList);

    CP_Discord_Send_HTTP_Close();

    pile.CP_Discord_Send.peer = NULL;
  }while(0);

  return 0;
}

NET_TCP_layerflag_t
CP_Discord_Send_read(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Discord_Send_pd_t *pd,
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
      PR_abort();
      __unreachable(); /* TOOD compiler is dumb */
    }
  }

  CP_Discord_Send_KeepAliveTimer_Update();

  uintptr_t ReadDataIndex = 0;

  while(1){
    hpall_ParsedData ParsedData;
    hpall_ParseType ParseType = hpall_Parse(&pd->hpall, ReadData, ReadSize, &ReadDataIndex, &ParsedData);
    if(ParseType == hpall_ParseType_Done){
      jsmn_parser jparser;
      jsmn_init(&jparser);
      jsmntok_t jtokens[128]; // TODO hardcode 128
      int jerr = jsmn_parse(&jparser, (const char *)pd->jvec.ptr, pd->jvec.Current, jtokens, 128); // TODO hardcode 128
      if(jerr < 0){
        PR_abort();
      }

      const uint32_t DepthTokenLimit = 32;
      struct{
        uint32_t Size;
        uint32_t Type;
        jsmntype_t jType;
      }DepthToken[DepthTokenLimit];
      uint32_t CurrentDepth = 0;

      if(CP_Discord_Send_PacketTypeList_Usage(&pd->PacketTypeList) == 0){
        PR_abort();
      }
      CP_Discord_Send_PacketType PacketType;
      CP_Discord_Send_PacketTypeList_NodeReference_t FirstPacketNR = CP_Discord_Send_PacketTypeList_GetNodeFirst(&pd->PacketTypeList);
      {
        CP_Discord_Send_PacketTypeList_Node_t *n = CP_Discord_Send_PacketTypeList_GetNodeByReference(&pd->PacketTypeList, FirstPacketNR);
        PacketType = n->data;
      }

      if(PacketType == CP_Discord_Send_PacketType_GetCurrentUser){
        // TODO
      }
      else if(PacketType == CP_Discord_Send_PacketType_CreateMessage){
        const uint32_t DepthType_Unknown = 0x00;
        const uint32_t DepthType_Begin = 0x01;
        const uint32_t DepthType_NoCare = 0x02;

        const uint32_t DepthType_ID = 0x03;

        bool Verified = false;

        for(uint32_t i = 0; i < jerr; i++){
          DepthToken[CurrentDepth].Size = jtokens[i].size;

          const void *tptr = (const void *)&((uint8_t *)pd->jvec.ptr)[jtokens[i].start];
          uint32_t tsize = jtokens[i].end - jtokens[i].start;

          if(CurrentDepth == 0){
            DepthToken[CurrentDepth].Type = DepthType_Begin;
          }
          else if(jtokens[i].type == JSMN_OBJECT || jtokens[i].type == JSMN_ARRAY){
            DepthToken[CurrentDepth].Type = DepthToken[CurrentDepth - 1].Type;
          }
          else if(DepthToken[CurrentDepth - 1].Type == DepthType_Begin){
            if(jtokens[i].type == JSMN_STRING){
              if(MEM_cstreu("id") == tsize && STR_ncmp("id", tptr, tsize) == false){
                Verified = true;
              }
              DepthToken[CurrentDepth].Type = DepthType_NoCare;
            }
            else{
              PR_abort();
            }
          }
          else if(DepthToken[CurrentDepth - 1].Type == DepthType_NoCare){
            DepthToken[CurrentDepth].Type = DepthType_NoCare;
          }
          else{
            print("else came %lx\n", DepthToken[CurrentDepth - 1].Type);
            if(jtokens[i].type == JSMN_STRING || jtokens[i].type == JSMN_PRIMITIVE){
              print("%lx %.*s %lu\n", jtokens[i].type, tsize, tptr, CurrentDepth);
            }
            PR_abort();
          }

          while(DepthToken[CurrentDepth].Size == 0){
            if(CurrentDepth == 0){
              /* all ended */
              goto gt_EndOfJSON_CreateMessage;
            }
            DepthToken[--CurrentDepth].Size--;
          }
          CurrentDepth++;
        }

        /* json parse should goto there instead */
        PR_abort();

        gt_EndOfJSON_CreateMessage:;
        if(Verified == true){
          VerifyMessage(CPType_Discord);
        }
        else{
          print("CP_Discord_Send Verify failed\n");
          NET_TCP_CloseHard(peer);
          return NET_TCP_EXT_PeerIsClosed_e;
        }
      }
      else{
        PR_abort();
      }

      CP_Discord_Send_PacketTypeList_unlrec(&pd->PacketTypeList, FirstPacketNR);

      CP_Discord_Send_HTTP_Reset();
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
