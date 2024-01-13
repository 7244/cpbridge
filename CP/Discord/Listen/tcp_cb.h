uint32_t
CP_Discord_Listen_connstate(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Discord_Listen_pd_t *pd,
  uint32_t flag
){
  if(flag & NET_TCP_state_succ_e){
    print("[+] CP_Discord_Listen\n");
    pile.CP_Discord_Listen.peer = peer;

    pd->Upgraded = false;

    CP_Discord_Listen_Init_BeforeUpgrade();

    NET_TCP_StartReadLayer(peer, pile.CP_Discord_Listen.LayerReadID);

    {
      VEC_t buf;
      VEC_init(&buf, 1, A_resize);

      VEC_print(&buf,
        "GET /?v=6&encoding=json HTTP/1.1\r\n"
        "Host: gateway.discord.gg\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: salsa\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n");

      NET_TCP_Queue_t Queue;
      Queue.DynamicPointer.ptr = buf.ptr;
      Queue.DynamicPointer.size = buf.Current;
      NET_TCP_write_loop(
        pile.CP_Discord_Listen.peer,
        NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Discord_Listen.peer),
        NET_TCP_QueueType_DynamicPointer_e,
        &Queue);

      VEC_free(&buf);
    }
  }
  else do{
    ConnectServer_TCP(&pile.CP_Discord_Listen);

    if(!(flag & NET_TCP_state_init_e)){
      break;
    }
    print("[-] CP_Discord_Listen Upgraded:%lx\n", pd->Upgraded);

    if(pd->Upgraded == true){
      print("  PacketTypeList_Usage() = %lx\n", CP_Discord_Listen_PacketTypeList_Usage(&pd->AfterUpgrade.PacketTypeList));

      CP_Discord_Listen_PacketTypeList_Close(&pd->AfterUpgrade.PacketTypeList);
      VEC_free(&pd->AfterUpgrade.jvec);

      if(pd->AfterUpgrade.KeepAliveStarted == true){
        EV_timer_stop(&pile.listener, &pd->AfterUpgrade.KeepAliveTimer);
      }
    }

    pile.CP_Discord_Listen.peer = NULL;
  }while(0);

  return 0;
}
void CP_Discord_Listen_SendKeepAlive(){
  CP_Discord_Listen_InsertPacketType(CP_Discord_Listen_PacketType_11);

  VEC_t vec;
  VEC_init(&vec, 1, A_resize);

  VEC_t dvec;
  VEC_init(&dvec, 1, A_resize);

  VEC_print(&dvec,
    "{"
      "\"op\": 1,"
      "\"d\": null"
    "}");

  Websock_WebsockHead_t Head;
  uint8_t HeadSize = Websock_MakeWebsockHead(&Head, dvec.Current);

  VEC_print(&vec, "%.*s%.*s", (uintptr_t)HeadSize, &Head, dvec.Current, dvec.ptr);

  VEC_free(&dvec);

  NET_TCP_Queue_t Queue;
  Queue.DynamicPointer.ptr = vec.ptr;
  Queue.DynamicPointer.size = vec.Current;
  NET_TCP_write_loop(
    pile.CP_Discord_Listen.peer,
    NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Discord_Listen.peer),
    NET_TCP_QueueType_DynamicPointer_e,
    &Queue);

  VEC_free(&vec);
}
void CP_Discord_Listen_KeepAliveTimer_cb(EV_t *listener, EV_timer_t *evt, uint32_t flag){
  CP_Discord_Listen_SendKeepAlive();
}
NET_TCP_layerflag_t
CP_Discord_Listen_read(
  NET_TCP_peer_t *peer,
  uint8_t *sd,
  CP_Discord_Listen_pd_t *pd,
  NET_TCP_QueuerReference_t QueuerReference,
  uint32_t *type,
  NET_TCP_Queue_t *Queue
){
  uint8_t *ReadData;
  uintptr_t ReadSize;
  uint8_t _EventReadBuffer[0x1000];
  switch(*type){
    case NET_TCP_QueueType_DynamicPointer_e:{
      ReadData = (uint8_t *)Queue->DynamicPointer.ptr;
      ReadSize = Queue->DynamicPointer.size;
      break;
    }
    case NET_TCP_QueueType_PeerEvent_e:{
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
    case NET_TCP_QueueType_CloseHard_e:{
      return 0;
    }
    default:{
      print("cb_read *type %lx\r\n", *type);
      PR_abort();
      __unreachable(); /* TOOD compiler is dumb */
    }
  }

  uintptr_t ReadDataIndex = 0;

  gt_ProcessRead:;

  if(pd->Upgraded == false){
    while(1){
      HTTP_result_t result;
      sint32_t state = HTTP_decode(&pd->BeforeUpgrade.HTTP_decode, ReadData, ReadSize, &ReadDataIndex, &result);
      if(state < 0){
        if(state == ~HTTP_DecodeError_Done_e){
          if(pd->BeforeUpgrade.HTTPCode != 101){
            PR_abort();
          }

          CP_Discord_Listen_Init_AfterUpgrade();

          goto gt_ProcessRead;
        }
        else{
          print("error state\n", state);
          PR_abort();
        }
      }
      else{
        switch(state){
          case HTTP_ResultType_NotEnoughData_e:{
            return 0;
          }
          case HTTP_ResultType_head_e:{
            pd->BeforeUpgrade.HTTPCode = STR_pss32(result.head.v[1], result.head.s[1]);
            break;
          }
          case HTTP_ResultType_header_e:{
            break;
          }
        }
      }
    }
  }
  else{ // websocket
    CP_Discord_Listen_PacketType pt = CP_Discord_Listen_GetPacketType();

    Websock_Error_t Error;
    uint64_t PayloadSize = Websock_Parse(&pd->AfterUpgrade.wparser, ReadData, ReadSize, &ReadDataIndex, &Error);
    VEC_print(&pd->AfterUpgrade.jvec, "%.*s", PayloadSize, &ReadData[ReadDataIndex - PayloadSize]);
    if(Error != 0){
      if(Error != Websock_Error_Done){
        PR_abort();
      }

      if(((uint8_t *)pd->AfterUpgrade.jvec.ptr)[0] != '{'){
        print("CP_Discord_Listen last packet was '%lx' this doesn't look like json:\n%.*s\n", pt, pd->AfterUpgrade.jvec.Current, pd->AfterUpgrade.jvec.ptr);
        NET_TCP_CloseHard(peer);
        return NET_TCP_EXT_PeerIsClosed_e;
      }

      jsmn_parser jparser;
      jsmn_init(&jparser);
      jsmntok_t jtokens[256];
      int jerr = jsmn_parse(
        &jparser,
        (const char *)pd->AfterUpgrade.jvec.ptr,
        pd->AfterUpgrade.jvec.Current,
        jtokens,
        256); // TODO hardcode 256
      if(jerr < 0){
        print("CP_Discord_Listen JSON parse failed %ld, json was:\n%.*s\n", jerr, pd->AfterUpgrade.jvec.Current, pd->AfterUpgrade.jvec.ptr);
        PR_abort();
      }

      const uint32_t DepthTokenLimit = 32;
      struct{
        uint32_t Size;
        uint32_t Type;
        jsmntype_t jType;
      }DepthToken[DepthTokenLimit];
      uint32_t CurrentDepth = 0;

      const uint32_t DepthType_Unknown = 0x00;
      const uint32_t DepthType_Begin = 0x01;
      const uint32_t DepthType_NoCare = 0x02;

      const uint32_t DepthType_t = 0x03;
      // const uint32_t DepthType_s = 0x04;
      const uint32_t DepthType_op = 0x05;
      const uint32_t DepthType_d = 0x06;

      const uint32_t DepthType_d_10 = 0x07;
      const uint32_t DepthType_d_10_HeartbeatInterval = 0x08;
      const uint32_t DepthType_d_0 = 0x09;
      const uint32_t DepthType_d_0_READY_SessionID = 0x0a;

      const uint32_t DepthType_d_0_MessageCreate_Author = 0x0b;
      const uint32_t DepthType_d_0_MessageCreate_Author_ID = 0x0c;
      const uint32_t DepthType_d_0_MessageCreate_Author_GlobalName = 0x0d;

      const uint32_t DepthType_d_0_MessageCreate_Content = 0x0e;
      const uint32_t DepthType_d_0_MessageCreate_ChannelID = 0x0f;

      struct{
        uint8_t t[128];
        uintptr_t tSize;
        uint64_t op;

        union{
          struct{
            bool Ignore; // false
            uint64_t AuthorID; // (uint64_t)-1
            uint8_t AuthorName[128];
            uint32_t AuthorNameSize; // 0
            uint8_t Content[0x2000];
            uint32_t ContentSize; // 0
          }MessageCreate;
        }Dispatch;
      }FrameCommon;
      FrameCommon.tSize = 0;
      FrameCommon.op = (uint64_t)-1;

      for(uint32_t i = 0; i < jerr; i++){
        DepthToken[CurrentDepth].Size = jtokens[i].size;

        const void *tptr = (const void *)&((uint8_t *)pd->AfterUpgrade.jvec.ptr)[jtokens[i].start];
        uint32_t tsize = jtokens[i].end - jtokens[i].start;

        if(CurrentDepth == 0){
          DepthToken[CurrentDepth].Type = DepthType_Begin;
        }
        else if(jtokens[i].type == JSMN_OBJECT || jtokens[i].type == JSMN_ARRAY){
          DepthToken[CurrentDepth].Type = DepthToken[CurrentDepth - 1].Type;
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_Begin){
          if(jtokens[i].type == JSMN_STRING){
            if(MEM_cstreu("t") == tsize && STR_ncmp("t", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_t;
            }
            else if(MEM_cstreu("s") == tsize && STR_ncmp("s", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_NoCare; // TODO
            }
            else if(MEM_cstreu("op") == tsize && STR_ncmp("op", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_op;
            }
            else if(MEM_cstreu("d") == tsize && STR_ncmp("d", tptr, tsize) == false){
              if(FrameCommon.op == 10){
                DepthToken[CurrentDepth].Type = DepthType_d_10;
              }
              else if(FrameCommon.op == 0){
                if(MEM_cstreu("READY") == FrameCommon.tSize && STR_ncmp("READY", FrameCommon.t, FrameCommon.tSize) == false){

                }
                else if(MEM_cstreu("MESSAGE_CREATE") == FrameCommon.tSize && STR_ncmp("MESSAGE_CREATE", FrameCommon.t, FrameCommon.tSize) == false){
                  FrameCommon.Dispatch.MessageCreate.Ignore = false;
                  FrameCommon.Dispatch.MessageCreate.AuthorID = (uint64_t)-1;
                  FrameCommon.Dispatch.MessageCreate.AuthorNameSize = 0;
                  FrameCommon.Dispatch.MessageCreate.ContentSize = 0;
                }
                DepthToken[CurrentDepth].Type = DepthType_d_0;
              }
              else{
                DepthToken[CurrentDepth].Type = DepthType_NoCare;
              }
            }
            else{
              DepthToken[CurrentDepth].Type = DepthType_NoCare;
            }
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_NoCare){
          DepthToken[CurrentDepth].Type = DepthType_NoCare;
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_t){
          if(jtokens[i].type == JSMN_PRIMITIVE || jtokens[i].type == JSMN_STRING){
            if(tsize > sizeof(FrameCommon.t)){
              PR_abort();
            }
            FrameCommon.tSize = tsize;
            MEM_copy(tptr, FrameCommon.t, tsize);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_op){
          if(jtokens[i].type == JSMN_PRIMITIVE){
            FrameCommon.op = STR_psu64(tptr, tsize);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_10){
          if(jtokens[i].type == JSMN_STRING){
            if(MEM_cstreu("heartbeat_interval") == tsize && STR_ncmp("heartbeat_interval", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_d_10_HeartbeatInterval;
            }
            else{
              DepthToken[CurrentDepth].Type = DepthType_NoCare;
            }
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_10_HeartbeatInterval){
          if(jtokens[i].type == JSMN_PRIMITIVE){
            if(pd->AfterUpgrade.KeepAliveStarted == true){
              PR_abort();
            }
            pd->AfterUpgrade.KeepAliveStarted = true;
            pd->AfterUpgrade.KeepAliveTimeMS = STR_psu64(tptr, tsize);
            EV_timer_init(
              &pd->AfterUpgrade.KeepAliveTimer,
              (f32_t)pd->AfterUpgrade.KeepAliveTimeMS / 1000 / 2,
              (EV_timer_cb_t)CP_Discord_Listen_KeepAliveTimer_cb);
            EV_timer_start(&pile.listener, &pd->AfterUpgrade.KeepAliveTimer);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0){
          if(MEM_cstreu("READY") == FrameCommon.tSize && STR_ncmp("READY", FrameCommon.t, FrameCommon.tSize) == false){
            if(jtokens[i].type == JSMN_STRING){
              if(MEM_cstreu("session_id") == tsize && STR_ncmp("session_id", tptr, tsize) == false){
                DepthToken[CurrentDepth].Type = DepthType_d_0_READY_SessionID;
              }
              else{
                DepthToken[CurrentDepth].Type = DepthType_NoCare;
              }
            }
            else{
              PR_abort();
            }
          }
          else if(MEM_cstreu("MESSAGE_CREATE") == FrameCommon.tSize && STR_ncmp("MESSAGE_CREATE", FrameCommon.t, FrameCommon.tSize) == false){
            if(jtokens[i].type == JSMN_STRING){
              if(MEM_cstreu("author") == tsize && STR_ncmp("author", tptr, tsize) == false){
                DepthToken[CurrentDepth].Type = DepthType_d_0_MessageCreate_Author;
              }
              else if(MEM_cstreu("content") == tsize && STR_ncmp("content", tptr, tsize) == false){
                DepthToken[CurrentDepth].Type = DepthType_d_0_MessageCreate_Content;
              }
              else if(MEM_cstreu("channel_id") == tsize && STR_ncmp("channel_id", tptr, tsize) == false){
                DepthToken[CurrentDepth].Type = DepthType_d_0_MessageCreate_ChannelID;
              }
              else{
                DepthToken[CurrentDepth].Type = DepthType_NoCare;
              }
            }
            else{
              PR_abort();
            }
          }
          else{
            DepthToken[CurrentDepth].Type = DepthType_NoCare;
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_READY_SessionID){
          if(jtokens[i].type == JSMN_STRING){
            if(tsize != set_CP_Discord_SessionID_Size){
              PR_abort();
            }
            SaveToFile("CP_Discord_SessionID", tptr, tsize);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_MessageCreate_Author){
          if(jtokens[i].type == JSMN_STRING){
            if(MEM_cstreu("id") == tsize && STR_ncmp("id", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_d_0_MessageCreate_Author_ID;
            }
            else if(MEM_cstreu("global_name") == tsize && STR_ncmp("global_name", tptr, tsize) == false){
              DepthToken[CurrentDepth].Type = DepthType_d_0_MessageCreate_Author_GlobalName;
            }
            else{
              DepthToken[CurrentDepth].Type = DepthType_NoCare;
            }
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_MessageCreate_Author_ID){
          if(jtokens[i].type == JSMN_STRING){
            FrameCommon.Dispatch.MessageCreate.AuthorID = STR_psu64(tptr, tsize);
            if(FrameCommon.Dispatch.MessageCreate.AuthorID == set_CP_Discord_Ignored_AuthorID){
              FrameCommon.Dispatch.MessageCreate.Ignore = true;
            }
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_MessageCreate_Author_GlobalName){
          if(jtokens[i].type == JSMN_STRING || jtokens[i].type == JSMN_PRIMITIVE){
            if(tsize > sizeof(FrameCommon.Dispatch.MessageCreate.AuthorName)){
              PR_abort();
            }

            FrameCommon.Dispatch.MessageCreate.AuthorNameSize = tsize;
            MEM_copy(tptr, FrameCommon.Dispatch.MessageCreate.AuthorName, tsize);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_MessageCreate_Content){
          if(jtokens[i].type == JSMN_STRING){
            if(tsize > sizeof(FrameCommon.Dispatch.MessageCreate.Content)){
              PR_abort();
            }

            FrameCommon.Dispatch.MessageCreate.ContentSize = tsize;
            MEM_copy(tptr, FrameCommon.Dispatch.MessageCreate.Content, tsize);
          }
          else{
            PR_abort();
          }
        }
        else if(DepthToken[CurrentDepth - 1].Type == DepthType_d_0_MessageCreate_ChannelID){
          if(jtokens[i].type == JSMN_STRING){
            if(MEM_cstreu(set_CP_Discord_ChannelID) != tsize || STR_ncmp(set_CP_Discord_ChannelID, tptr, tsize) != false){
              FrameCommon.Dispatch.MessageCreate.Ignore = true;
            }
            DepthToken[CurrentDepth].Type = DepthType_Unknown;
          }
          else{
            PR_abort();
          }
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
            goto gt_EndOfJSON;
          }
          DepthToken[--CurrentDepth].Size--;
        }
        CurrentDepth++;
      }

      PR_abort();

      gt_EndOfJSON:;

      if(FrameCommon.op == 10){
        CP_Discord_Listen_InsertPacketType(CP_Discord_Listen_PacketType_2);

        VEC_t vec;
        VEC_init(&vec, 1, A_resize);

        VEC_t dvec;
        VEC_init(&dvec, 1, A_resize);

        {
          uint8_t SessionID[set_CP_Discord_SessionID_Size];
          // TOOD use resume
          if(0 && ReadFromFile("CP_Discord_SessionID", SessionID, set_CP_Discord_SessionID_Size) == true){
            VEC_print(&dvec,
              "{"
                "\"op\": 6,"
                "\"d\": {"
                  "\"token\": \"" set_CP_Discord_Token "\","
                  "\"session_id\": \"%.*s\","
                  "\"seq\": 0"
                "}"
              "}"
            , set_CP_Discord_SessionID_Size, SessionID);
          }
          else{
            VEC_print(&dvec,
              "{"
                "\"op\": 2,"
                "\"d\": {"
                  "\"token\": \"" set_CP_Discord_Token "\","
                  "\"intents\": 512,"
                  "\"properties\": {"
                    "\"os\": \"something\","
                    "\"browser\": \"wfb\","
                    "\"device\": \"salsa\""
                  "}"
                "}"
              "}"
            );
          }
        }


        Websock_WebsockHead_t Head;
        uint8_t HeadSize = Websock_MakeWebsockHead(&Head, dvec.Current);

        VEC_print(&vec, "%.*s%.*s", (uintptr_t)HeadSize, &Head, dvec.Current, dvec.ptr);

        VEC_free(&dvec);

        NET_TCP_Queue_t Queue;
        Queue.DynamicPointer.ptr = vec.ptr;
        Queue.DynamicPointer.size = vec.Current;
        NET_TCP_write_loop(
          pile.CP_Discord_Listen.peer,
          NET_TCP_GetWriteQueuerReferenceFirst(pile.CP_Discord_Listen.peer),
          NET_TCP_QueueType_DynamicPointer_e,
          &Queue);

        VEC_free(&vec);
      }
      else if(FrameCommon.op == 11){
        if(pt != CP_Discord_Listen_PacketType_11){
          PR_abort();
        }
        CP_Discord_Listen_RemovePacketType();
      }
      else if(FrameCommon.op == 0){
        if(MEM_cstreu("READY") == FrameCommon.tSize && STR_ncmp("READY", FrameCommon.t, FrameCommon.tSize) == false){
          if(pt == CP_Discord_Listen_PacketType_2){
            CP_Discord_Listen_RemovePacketType();
            BroadcastMessages_Discord();
          }
          else{
            PR_abort();
          }
        }
        else if(MEM_cstreu("MESSAGE_CREATE") == FrameCommon.tSize && STR_ncmp("MESSAGE_CREATE", FrameCommon.t, FrameCommon.tSize) == false){
          if(
            FrameCommon.Dispatch.MessageCreate.Ignore == false &&
            FrameCommon.Dispatch.MessageCreate.ContentSize != 0
          ){
            VEC_t DeescapedMessage;
            VEC_init(&DeescapedMessage, 1, A_resize);
            CP_Discord_DeescapeString(
              FrameCommon.Dispatch.MessageCreate.Content,
              FrameCommon.Dispatch.MessageCreate.ContentSize,
              &DeescapedMessage);

            if(DeescapedMessage.Current == 0){
              VEC_free(&DeescapedMessage);
            }
            else{
              MessageList_NodeReference_t mnr = MessageList_NewNodeLast(&MessageList);
              MessageList_Node_t *mn = MessageList_GetNodeByReference(&MessageList, mnr);

              mn->data.CPFrom = CPType_Discord;
              mn->data.CPReferenceCount = TotalCPType;
              mn->data.UserID = FrameCommon.Dispatch.MessageCreate.AuthorID;

              {
                VEC_t DeescapedAuthorName;
                VEC_init(&DeescapedAuthorName, 1, A_resize);
                CP_Discord_DeescapeString(
                  FrameCommon.Dispatch.MessageCreate.AuthorName,
                  FrameCommon.Dispatch.MessageCreate.AuthorNameSize,
                  &DeescapedAuthorName);
                mn->data.NameSize = FrameCommon.Dispatch.MessageCreate.AuthorNameSize;
                MEM_copy(
                  DeescapedAuthorName.ptr,
                  mn->data.Name,
                  mn->data.NameSize >= set_NameSizeLimit ? set_NameSizeLimit : mn->data.NameSize);
                VEC_free(&DeescapedAuthorName);
              }

              mn->data.TextSize = DeescapedMessage.Current;
              mn->data.Text = DeescapedMessage.ptr;

              BroadcastNewMessages();
            }
          }
        }
        else{
          /* unknown dispatch */
        }
      }

      CP_Discord_Listen_Reinit_AfterUpgrade();
      goto gt_ProcessRead;
    }
  }

  return 0;
}
