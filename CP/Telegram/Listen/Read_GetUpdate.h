enum{
  DepthType_Unknown,

  DepthType_NoCare,

  DepthType_Begin,

  DepthType_OK,

  DepthType_Result,

  DepthType_UpdateID,
  DepthType_Message,

  DepthType_Message_MessageID,

  DepthType_Message_From,
  DepthType_Message_From_ID,
  DepthType_Message_From_IsBot,
  DepthType_Message_From_FirstName,

  DepthType_Message_Chat,
  DepthType_Message_Chat_ID,
  DepthType_Message_Chat_Title,
  DepthType_Message_Chat_Type,
  DepthType_Message_Chat_AllMembersAreAdministrators,

  DepthType_Message_Date,

  DepthType_Message_Text
};

struct{
  bool Ignore; // false
  sint64_t TelegramID; // 0
  const void *Text;
  uint32_t TextSize; // 0
  const void *Name;
  uint32_t NameSize; // 0
}Message;

for(uint32_t i = 0; i < jerr; i++){
  DepthToken[CurrentDepth].Size = jtokens[i].size;
  DepthToken[CurrentDepth].jType = jtokens[i].type;

  const void *tptr = (const void *)&((uint8_t *)pd->jvec.ptr)[jtokens[i].start];
  uint32_t tsize = jtokens[i].end - jtokens[i].start;

  if(CurrentDepth == 0){
    DepthToken[CurrentDepth].Type = DepthType_Begin;
  }
  else if(DepthToken[CurrentDepth].jType == JSMN_OBJECT || DepthToken[CurrentDepth].jType == JSMN_ARRAY){
    DepthToken[CurrentDepth].Type = DepthToken[CurrentDepth - 1].Type;
    if(DepthToken[CurrentDepth].Type == DepthType_Result && DepthToken[CurrentDepth].jType == JSMN_OBJECT){
      Message.Ignore = false;
      Message.TelegramID = 0;
      Message.TextSize = 0;
      Message.NameSize = 0;
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Begin){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      if(MEM_cstreu("ok") == tsize && STR_ncmp("ok", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_OK;
      }
      else if(MEM_cstreu("result") == tsize && STR_ncmp("result", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Result;
      }
      else{
        PR_abort();
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_OK){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      if(tsize != 0 && ((uint8_t *)tptr)[0] == 't'){
        DepthToken[CurrentDepth].Type = DepthType_Unknown;
      }
      else{
        print("telegram ded\n%.*s\n%.*s\n", tsize, tptr, pd->jvec.Current, pd->jvec.ptr);
        PR_abort();
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Result){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      if(MEM_cstreu("update_id") == tsize && STR_ncmp("update_id", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_UpdateID;
      }
      else if(MEM_cstreu("message") == tsize && STR_ncmp("message", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message;
      }
      else{
        DepthToken[CurrentDepth].Type = DepthType_NoCare;
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_UpdateID){
    uint64_t tuid = STR_psu64(tptr, tsize);
    if(tuid > pile.CP_Telegram_UpdateID){
      pile.CP_Telegram_UpdateID = tuid;
    }
    DepthToken[CurrentDepth].Type = DepthType_Unknown;
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      if(MEM_cstreu("message_id") == tsize && STR_ncmp("message_id", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_MessageID;
      }
      else if(MEM_cstreu("from") == tsize && STR_ncmp("from", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_From;
      }
      else if(MEM_cstreu("chat") == tsize && STR_ncmp("chat", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Chat;
      }
      else if(MEM_cstreu("date") == tsize && STR_ncmp("date", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Date;
      }
      else if(MEM_cstreu("text") == tsize && STR_ncmp("text", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Text;
      }
      else{
        DepthToken[CurrentDepth].Type = DepthType_NoCare;
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_MessageID){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      // TOOD what is useful for?
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_From){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      if(MEM_cstreu("id") == tsize && STR_ncmp("id", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_From_ID;
      }
      else if(MEM_cstreu("is_bot") == tsize && STR_ncmp("is_bot", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_From_IsBot;
      }
      else if(MEM_cstreu("first_name") == tsize && STR_ncmp("first_name", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_From_FirstName;
      }
      else{
        DepthToken[CurrentDepth].Type = DepthType_NoCare;
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_From_ID){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      Message.TelegramID = STR_pss32(tptr, tsize);
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_From_IsBot){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_From_FirstName){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      Message.Name = tptr;
      Message.NameSize = tsize;
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Chat){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      if(MEM_cstreu("id") == tsize && STR_ncmp("id", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Chat_ID;
      }
      else if(MEM_cstreu("title") == tsize && STR_ncmp("title", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Chat_Title;
      }
      else if(MEM_cstreu("type") == tsize && STR_ncmp("type", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Chat_Type;
      }
      else if(MEM_cstreu("all_members_are_administrators") == tsize && STR_ncmp("all_members_are_administrators", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_Message_Chat_AllMembersAreAdministrators;
      }
      else{
        PR_abort();
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Chat_ID){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      if(MEM_cstreu(set_CP_Telegram_GroupID) != tsize || STR_ncmp(set_CP_Telegram_GroupID, tptr, tsize) != false){
        Message.Ignore = true;
      }
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Chat_Title){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Chat_Type){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Chat_AllMembersAreAdministrators){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Date){
    if(DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      // TOOD use it
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_Message_Text){
    if(DepthToken[CurrentDepth].jType == JSMN_STRING){
      Message.Text = tptr;
      Message.TextSize = tsize;
      DepthToken[CurrentDepth].Type = DepthType_Unknown;
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_NoCare){
    DepthToken[CurrentDepth].Type = DepthType_NoCare;
  }
  else{
    print("else came %lx\n", DepthToken[CurrentDepth].Type);
    if(DepthToken[CurrentDepth].jType == JSMN_STRING || DepthToken[CurrentDepth].jType == JSMN_PRIMITIVE){
      print("%lx %.*s %lu\n", DepthToken[CurrentDepth].jType, tsize, tptr, CurrentDepth);
    }
    PR_abort();
  }

  while(DepthToken[CurrentDepth].Size == 0){
    if(
      DepthToken[CurrentDepth].Type == DepthType_Result && DepthToken[CurrentDepth].jType == JSMN_OBJECT &&
      Message.Ignore == false
    ){
      VEC_t DeescapedText;
      VEC_init(&DeescapedText, 1, A_resize);
      CP_Telegram_DeescapeString(Message.Text, Message.TextSize, &DeescapedText);
      if(DeescapedText.Current == 0){
        VEC_free(&DeescapedText);
      }
      else{
        MessageList_NodeReference_t mnr = MessageList_NewNodeLast(&MessageList);
        MessageList_Node_t *mn = MessageList_GetNodeByReference(&MessageList, mnr);
        mn->data.CPFrom = CPType_Telegram;
        mn->data.CPReferenceCount = TotalCPType;
        mn->data.UserID = Message.TelegramID;

        {
          VEC_t DeescapedName;
          VEC_init(&DeescapedName, 1, A_resize);
          CP_Telegram_DeescapeString(Message.Name, Message.NameSize, &DeescapedName);
          mn->data.NameSize = DeescapedName.Current;
          MEM_copy(
            DeescapedName.ptr,
            mn->data.Name,
            mn->data.NameSize >= set_NameSizeLimit ? set_NameSizeLimit : mn->data.NameSize
          );
          VEC_free(&DeescapedName);
        }

        mn->data.TextSize = DeescapedText.Current;
        mn->data.Text = DeescapedText.ptr;
        BroadcastNewMessages();
      }
    }
    if(CurrentDepth == 0){
      /* all ended */
      goto gt_EndOfJSON_GetUpdate;
    }
    DepthToken[--CurrentDepth].Size--;
  }
  CurrentDepth++;
}

/* json parse should goto there instead */
PR_abort();

gt_EndOfJSON_GetUpdate:;
CP_Telegram_Listen_GetUpdate();
