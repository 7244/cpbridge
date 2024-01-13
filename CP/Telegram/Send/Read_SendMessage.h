const uint32_t DepthType_Unknown = 0x00;
const uint32_t DepthType_Begin = 0x01;
const uint32_t DepthType_OK = 0x02;

const uint32_t DepthType_NoCare = 0x03;

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
      if(MEM_cstreu("ok") == tsize && STR_ncmp("ok", tptr, tsize) == false){
        DepthToken[CurrentDepth].Type = DepthType_OK;
      }
      else{
        DepthToken[CurrentDepth].Type = DepthType_NoCare;
      }
    }
    else{
      PR_abort();
    }
  }
  else if(DepthToken[CurrentDepth - 1].Type == DepthType_OK){
    if(jtokens[i].type == JSMN_PRIMITIVE){
      if(tsize != 0 && ((uint8_t *)tptr)[0] == 't'){
        VerifyMessage(CPType_Telegram);
        DepthToken[CurrentDepth].Type = DepthType_Unknown;
      }
      else{
        PR_abort();
      }
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
      goto gt_EndOfJSON_SendMessage;
    }
    DepthToken[--CurrentDepth].Size--;
  }
  CurrentDepth++;
}

/* json parse should goto there instead */
PR_abort();

gt_EndOfJSON_SendMessage:;
