/**
Ross Wagner
4/15/2018

This program takes in tokens generated by lexical.c and determines if the
PL/0 program is well formed (syntatically correct)




*/


#include "codes.h"
#include "parser.h"

// global vars
extern symbol symbolTable[MAX_SYMBOL_TABLE_SIZE];
extern int symbolTableIndex;

// global error node
extern token errorToke;
extern node errorNode;

// error flag
extern int errorFlag;
// where intermidiate code from generated in parser will go
extern FILE* codeFile;

// where errors are stored. global so I dont have to
extern FILE* errorFile;

extern instruction aCode[MAX_CODE_LENGTH];
extern int aCodeIndex;

int lexicalLevel = 0;
int sp[MAX_LEXI_LEVLES];

// keeps track of open registers. 0 for open 1 for occupied
extern int regStatus[NUMREG];

/*
// for testing
int main(int argc, char** argv){

  if(argc != 2){
    printf("Usage: ./parse <PM0 source file>\n" );
    return ERROR;
  }

  errorFile = fopen("errors.txt", "w");
  if(!errorFile){
    printf("Failed to open errors.txt");
  }
  // get lexem list
  char * code = fileNameToStr(argv[1]);

  if(code == NULL){
    printf("Failed to read file");
    return ERROR;
  }

  // init the error node
  strcpy(errorToke.text,"");
  errorToke.atribute = -1;
  errorNode.token = errorToke;
  errorNode.next = NULL;
  errorNode.prev = NULL;

  //printf("%s", code);

  node * lexTable = NULL;
  lexTable = makeLexTable(&code);

  parse(lexTable);

}
*/

/**
@params current node in a linked list of token nodes

@returns an int, a status code
*/
int block(node * current){
  // const declaration
  if (current->token.atribute == constsym){
    int atribute = current->token.atribute;
    do {
      // init new const sym
      symbol newSym;
      newSym.atribute=atribute;
      newSym.level = lexicalLevel;

      //make room on stack
      newSym.addr = sp[lexicalLevel]++;
      gen(inc, 0 ,0,1);


      *current = *getNextLex(current);
      if (current->token.atribute != identsym){
        error(27,current->token.line);
        return ERROR;
      }
      // set name
      strcpy(newSym.name, current->token.text);

      *current = *getNextLex(current);
      if (current->token.atribute != eqsym){
        error(3,current->token.line);
        return ERROR;
      }

      *current = *getNextLex(current);
      if(current->token.atribute != numbersym){
        error(2,current->token.line);
        return ERROR;
      }
      // set value
      newSym.value = (int) strtol(current->token.text, (char **)NULL, 10);

      // add to symbol table
      enter(newSym);

      // gen the code
      // store number in register
      int r = getNextOpenReg();
      // set reg to occupied
      regStatus[r] = 1;
      gen(lit, r, 0, newSym.value);
      gen(sto, r, 0, newSym.addr);
      regStatus[r] = 0;

      *current = *getNextLex(current);

    } while(current->token.atribute == commasym);

    if(current->token.atribute != semicolonsym){
      error(5, current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);
  }

  // var declaration
  if(current->token.atribute == intsym || current->token.atribute == varsym){
    int atribute = current->token.atribute;
    do {
      symbol newSym;
      newSym.atribute = atribute;
      newSym.level = lexicalLevel;
      newSym.addr = sp[lexicalLevel]++;
      gen(inc,0,0,1);
      newSym.value = 0;

      *current = *getNextLex(current);
      if (current->token.atribute != identsym){
        error(27,current->token.line);
        return ERROR;
      }

      // coppy text to new symbol
      strcpy(newSym.name, current->token.text);


      *current = *getNextLex(current);

      // add to symbol table
      enter(newSym);


    } while(current->token.atribute == commasym);

    if(current->token.atribute != semicolonsym){
      error(5,current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);
  }

  // procedure declaration
  while(current->token.atribute == procsym){
    *current = *getNextLex(current);
    if(current->token.atribute != identsym){
      error(27, current->token.line);
      return ERROR;
    }

    // create and add new symbol
    symbol newSym;
    newSym.atribute = procsym;
    strcpy(newSym.name, current->token.text);
    newSym.level = lexicalLevel;
    //newSym.addr = getNextOpenReg();
    newSym.addr = sp[lexicalLevel]++;
    gen(inc,0,0,1);
    newSym.value = 0;
    enter(newSym);

    *current = *getNextLex(current);
    if(current ->token.atribute = semicolonsym){
      error(5, current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);

    // jump over to start
    int jumpLoc = aCodeIndex;
    gen(jmp, 0,0,-1);

    lexicalLevel++;

    // make sure max leixal level not exceeded
    if(lexicalLevel >= MAX_LEXI_LEVLES){
      error(37,current->token.line);
      return ERROR;
    }

    block(current);
    lexicalLevel--;

    aCode[jumpLoc].m = aCodeIndex;
    // open any registers used by symbols at lexical levle +1
    //freeReg(lexicalLevel+1);

    if(current ->token.atribute = semicolonsym){
      error(5, current->token.line);
      return ERROR;
    }
  }

  return statement(current);
}

int condition(node * current){
  // odd
  if(current->token.atribute == oddsym){
    *current = *getNextLex(current);

    // where result of expression will be stored
    int r1 = getNextOpenReg();
    expression(current);
    if(errorFlag){
      return ERROR;
    }

    //check if content of r1 is odd
    gen(odd, r1,r1,0);
  }
  else{
    int r1 = getNextOpenReg();
    expression(current);
    if(errorFlag){
      return ERROR;
    }

    //close the reg
    regStatus[r1] = 1;

    if(!(current->token.atribute == eqsym || current->token.atribute ==neqsym ||
      current->token.atribute ==lessym || current->token.atribute == leqsym ||
      current->token.atribute == gtrsym || current->token.atribute == geqsym)){

      error(20, current->token.line);
      return ERROR;

    }
    // because they are in the same order
    int op = (current->token.atribute) - eqsym + eql;

    *current = *getNextLex(current);
    int r2 = getNextOpenReg();
    expression(current);
    if(errorFlag){
      return ERROR;
    }

    // compare results and put it in r1
    gen(op,r1, r1, r2);

    // reopen register
    regStatus[r1] = 0;
  }

  return OK;
}

/***/
int enter(symbol newSym){
  if(symbolTableIndex >= MAX_SYMBOL_TABLE_SIZE){
    error(29,newSym.addr);
    return ERROR;
  }
  else{
    symbolTable[symbolTableIndex++] = newSym;
    return OK;
  }
}

/**
code meaning for the error function
1. Use = instead of :=.
2. = must be followed by a number.
3. Identifier must be followed by =.
4. const, var, procedure must be followed by identifier.
5. Semicolon or comma missing.
6. Incorrect symbol after procedure declaration.
7. Statement expected.
8. Incorrect symbol after statement part in block.
9. Period expected.
10. Semicolon between statements missing.
11. Undeclared identifier.
12. Assignment to constant or procedure is not allowed.
13. Assignment operator expected.
14. call must be followed by an identifier.
15. Call of a constant or variable is meaningless.
16. then expected.
17. Semicolon or } expected.
18. do expected.
19. Incorrect symbol following statement.
20. Relational operator expected.
21. Expression must not contain a procedure identifier.
22. Right parenthesis missing.
23. The preceding factor cannot begin with this symbol.
24. An expression cannot begin with this symbol.
25. This number is too large.
26. Unexpected end of file/lexeme table
27. Identifier expected
28. := expected.
29. Max symbol table size exceeded
30. begin must be closed with end
31. if condition must be followed by then
32. while condition must be followed by do
33. identifier, (, or number expected.
34. Max code length exceeded
35. All registers in use, cannot give variable address.
36. Constant or variable expected.
37. Max lexical depth exceeded.
*/
void error(int eCode, int line){

  fprintf(errorFile,"Error line %d: ", line);

  switch(eCode){
    case 1:
      fprintf(errorFile,"Use = instead of :=.\n");
      break;
    case 2:
      fprintf(errorFile,"= must be followed by a number.\n");
      break;
    case 3:
      fprintf(errorFile,"Identifier must be followed by =.\n");
      break;
    case 4:
      fprintf(errorFile,"const, var, procedure must be followed by identifier.\n");
      break;
    case 5:
      fprintf(errorFile,"Semicolon or comma missing.\n");
      break;
    case 6:
      fprintf(errorFile, "Incorrect symbol after procedure declaration.\n");
      break;
    case 7:
      fprintf(errorFile, "Statement expected.\n");
      break;
    case 8:
      fprintf(errorFile, "Incorrect symbol after statement part in block.\n");
      break;
    case 9:
      fprintf(errorFile,"Period expected.\n");
      break;
    case 10:
      fprintf(errorFile,"Semicolon between statements missing.\n");
      break;
    case 11:
      fprintf(errorFile,"Undeclared identifier.\n");
      break;
    case 12:
      fprintf(errorFile,"Assignment to constant or procedure is not allowed\n");
      break;
    case 13:
      fprintf(errorFile,"Assignment operator expected.\n");
      break;
    case 14:
      fprintf(errorFile,"call must be followed by an identifier.\n");
      break;
    case 15:
      fprintf(errorFile,"Call of a constant or variable is meaningless.\n");
      break;
    case 16:
      fprintf(errorFile,"then expected.\n");
      break;
    case 17:
      fprintf(errorFile,"Semicolon or } expected.\n");
      break;
    case 18:
      fprintf(errorFile,"do expected.\n");
      break;
    case 19:
      fprintf(errorFile,"Incorrect symbol following statement.\n");
      break;
    case 20:
      fprintf(errorFile,"Relational operator expected.\n");
      break;
    case 21:
      fprintf(errorFile,"Expression must not contain a procedure identifier.\n");
      break;
    case 22:
      fprintf(errorFile,"Right parenthesis missing.\n");
      break;
    case 23:
      fprintf(errorFile,"The preceding factor cannot begin with this symbol.\n");
      break;
    case 24:
      fprintf(errorFile,"An expression cannot begin with this symbol.\n");
      break;
    case 25:
      fprintf(errorFile,"This number is too large.\n");
      break;
    case 26:
      fprintf(errorFile, "Unexpected end of file/program/lexem table.\n" );
      break;
    case 27:
      fprintf(errorFile,"Identifier expected.\n");
      break;
    case 28:
      fprintf(errorFile,":= expected.\n");
      break;
    case 29:
      fprintf(errorFile, "Max symbol table size exeded.\n");
      break;
    case 30:
      fprintf(errorFile, "begin must be closed with end.\n" );
      break;
    case 31:
      fprintf(errorFile, "if condition must be followed by then.\n" );
      break;
    case 32:
      fprintf(errorFile, "while condition must be followed by do.\n" );
      break;
    case 33:
      fprintf(errorFile, "identifier, (, or number expected.\n" );
      break;
    case 34:
      fprintf(errorFile, "Max code length exceeded.\n" );
      break;
    case 35:
      fprintf(errorFile, "All registers in use.\n" );
      break;
    case 36:
      fprintf(errorFile, "Constant or variable expected.\n" );
      break;
    case 37:
      fprintf(errorFile, "Max lexical depth exceeded.\n" );
      break;


  }
  errorFlag = 1;
}

/*
+/- chains
*/
int expression(node * current){
  int negate = 0;
  if (negate =current->token.atribute == plussym || current->token.atribute == minussym){

    *current = *getNextLex(current);
  }

  // remember to negater if minus
  int r1 = getNextOpenReg();
  term(current);
  if(errorFlag){
    return ERROR;
  }

  if (negate){
    gen(neg, r1,r1,0);
  }

  // close register b/c we are about to start working with more numbers
  regStatus[r1]=1;

  int doAdd;
  while( (doAdd = current->token.atribute == plussym) || current->token.atribute == minussym){

    *current = *getNextLex(current);
    int r2 = getNextOpenReg();
    term(current);
    if(errorFlag){
      return ERROR;
    }

    // add or subtract new term
    if(doAdd){
     gen(add, r1,r1,r2);
    }
    else{
      gen(sub, r1,r1,r2);
    }

  }
  regStatus[r1] = 0;

  return OK;
}

int factor(node * current){

  // variable name
  if(current->token.atribute == identsym){
    // find where var is stored
    int index = find(current->token.text);
    if(index == -1){
      error(11, current->token.line);
      return ERROR;
    }
    int type = symbolType(index);
    if (type == varsym || type == intsym){
      // get val and load into reg
      gen(lod,getNextOpenReg(),0,symbolTable[index].addr);
    }
    else if (type == constsym){
      gen(lit,getNextOpenReg(), 0,  symbolTable[index].value);
    }
    else{
      error(36, current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);
  }

  // just a number
  else if(current->token.atribute == numbersym){
    gen(lit, getNextOpenReg(),0, (int) strtol(current->token.text, (char **)NULL, 10) );
    *current = *getNextLex(current);

  }

  // parenthesis
  else if(current->token.atribute == lparentsym){
    *current = *getNextLex(current);

    expression(current);

    if(errorFlag){
      return ERROR;
    }

    if(current->token.atribute != rparentsym){
      error(22,current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);

  }

  else{
    error(33,current->token.line);
    return ERROR;
  }

  return OK;
}

/**
returns the index of the symbol with the given identifier. returns -1 if not found
*/
int find(char *ident){
  int i;
  for(i = 0; i < symbolTableIndex; i++){
    if(strcmp(ident, symbolTable[i].name) == 0){
      return i;
    }
  }
  // not found
  return -1;
}

/**

*/
void freeReg(int lexLevel){
  int i;
  for(i = 0; i <symbolTableIndex; i++){
    if(symbolTable[i].level >= lexLevel){
      regStatus[symbolTable[i].addr] = 0;
    }
  }
}

/**
generates an assembly instruction for PM0 and puts it into the codeFile
*/
void gen(int op, int reg, int l, int m){
  //fprintf(codeFile, "%d %d %d %d\n", op, reg,l,m );
  if(aCodeIndex >= MAX_CODE_LENGTH){
    error(34, -1);
    return;
  }

  instruction temp;
  temp.op = op;
  temp.r = reg;
  temp.l = l;
  temp.m = m;

  aCode[aCodeIndex++] = temp;
}

node * getNextLex(node * current){
  node * next = current->next;

  // check if tail sentinal node
  if(next == NULL || next->next == NULL){
    //error unexpected end
    error(26, current->token.line);

    // create dummy error node
    return &errorNode;
  }
  else{
    // debug
    //printf("nextTok: %s atribute: %d\n", next->token.text, next->token.atribute);

    return next;
  }
}

/*
returns the index if the lowest open register. returns ERROR and calls erroe()
if no registersa are open
*/
int getNextOpenReg(){
  int i;
  for(i = 0; i < NUMREG; i++){
    if(regStatus[i] == 0){
      return i;
    }
  }
  error(35, -1);
  return ERROR;

}

int parse(node * lexTable){
  node * current = getNextLex(lexTable);
  *current = *getNextLex(lexTable);

  // create space for activation fraem
  gen(inc, 0, 0, 4);
  sp[lexicalLevel] = 4;

  block(current);

  if (errorFlag){
    return ERROR;
  }

  if (current->token.atribute != periodsym){
    error(9, current->token.line);
    return ERROR;
  }

  // end of Program
  gen(sio, 0,0,3);
  return OK;

}

void printSymbolTable(){
  int i;
  for(i = 0; i < symbolTableIndex; i++){
    symbol sym = symbolTable[i];
    printf("atribute: %d name: %s value: %d levle: %d addr: %d\n",
      sym.atribute,sym.name,sym.value,sym.level,sym.addr);
  }
}

/**

calls condition, expression, and its self. is called by block.

deals with Assignments, call, begin, if, read, write, and while



*/
int statement(node * current){
  int status = OK;
  // Assignment statement
  if(current->token.atribute == identsym){
    // location in symbol table of current identifier
    int loc = find(current->token.text);
    // make sure identifier is declared
    if (loc == -1){
      error(11, current->token.line);
      return ERROR;
    }


    *current = *getNextLex(current);
    if(current->token.atribute != becomessym){
      error(28, current->token.line);
      return ERROR;
    }
    *current = *getNextLex(current);

    // location where value of expression will eventualy be stored. dont close here
    int r = getNextOpenReg();

    status = expression(current);
    if(status != OK){
      return status;
    }

    gen(sto, r, symbolTable[loc].level , symbolTable[loc].addr);
  }

  // read
  else if (current->token.atribute == readsym){
    *current = *getNextLex(current);
    // expect an identifier
    if(current->token.atribute != identsym){
      error(27, current->token.line);
      return ERROR;
    }

    // get address and lexical level of identifier
    int index = find(current->token.text);
    int l = symbolTable[index].level;
    int addr = symbolTable[index].addr;



    *current = *getNextLex(current);
    // expect  ;
    if(current->token.atribute != semicolonsym){
      error(5, current->token.line);
      return ERROR;
    }
    // generate code to read input here
    gen(sio, getNextOpenReg(), 0, 2);
    gen(sto, getNextOpenReg(),l,addr);
  }

  // write
  else if (current->token.atribute == writesym){
    *current = *getNextLex(current);
    // expect an identifier
    if(current->token.atribute != identsym){
      error(27, current->token.line);
      return ERROR;
    }
    // find symbol with identifier
    int symIndex = find(current->token.text);

    *current = *getNextLex(current);
    // expect  ;
    if(current->token.atribute != semicolonsym){
      error(5, current->token.line);
      return ERROR;
    }
    // generate code to write output here
    // put what is at identifiers address into register
    int r = getNextOpenReg();
    regStatus[r] =1;
    gen(lod, r, symbolTable[symIndex].level, symbolTable[symIndex].addr);
    gen(sio, r, 0, 1);
    regStatus[r]= 0;
  }

  // procedure call
  else if(current->token.atribute == callsym){
    *current = *getNextLex(current);
    if(current->token.atribute != identsym){
      error(27, current->token.line);
      return ERROR;
    }

    // location in symbol table of current identifier
    int loc = find(current->token.text);
    // make sure identifier is declared
    if (loc == -1){
      error(11, current->token.line);
      return ERROR;
    }

    gen(cal, 0, symbolTable[loc].level , symbolTable[loc].addr);


    *current = *getNextLex(current);
  }

  // begin
  else if(current->token.atribute == beginsym){
    *current = *getNextLex(current);
    status = statement(current);
    if(errorFlag){
      return ERROR;
    }
    while(current->token.atribute == semicolonsym){
      *current = *getNextLex(current);
      statement(current);
      if(errorFlag){
        return ERROR;
      }
    }
    if (current->token.atribute != endsym){
      error(30,current->token.line);
      return ERROR;
    }
    *current = *getNextLex(current);
  }

  // if statement
  else if(current->token.atribute == ifsym){
    *current = *getNextLex(current);
    // location where result of condition will be stored
    int r1 = getNextOpenReg();
    condition(current);
    if(errorFlag){
      return ERROR;
    }

    // -1 is a placeholder
    int jpLoc = aCodeIndex;
    gen(jpc, r1, 0, -1);

    if (current->token.atribute != thensym){
      error(31,current->token.line);
      return ERROR;
    }
    *current = *getNextLex(current);

    statement(current);
    if(errorFlag){
      return ERROR;
    }

    // set jump value
    aCode[jpLoc].m = aCodeIndex;
  }

  // while statement
  else if(current->token.atribute == whilesym){
    *current = *getNextLex(current);

    // where result of condition will be stored
    int r1 = getNextOpenReg();
    //location to jump back to
    int loopLoc = aCodeIndex;

    condition(current);
    if(errorFlag){
      return ERROR;
    }

    int jpcLoc = aCodeIndex;
    // -1 tepm
    gen(jpc, r1, 0,-1);

    if(current->token.atribute != dosym){
      error(33, current->token.line);
      return ERROR;
    }

    *current = *getNextLex(current);
    statement(current);
    if(errorFlag){
      return ERROR;
    }

    // jump back to conditional checks
    gen(jmp, 0,0, loopLoc);

    // set location to jump to if condition is false
    aCode[jpcLoc].m = aCodeIndex;

  }


  return status;
}


int symbolAddress(int index){
  if(index < 0 || index >= symbolTableIndex){
    return -1;
  }
  else return symbolTable[index].addr;
}

/**
returns the lexical levle the symbol at given index is on. -1 if index is out of bounds
*/
int symbolLevel(int index){
  if(index < 0 || index >= symbolTableIndex){
    return -1;
  }
  else return symbolTable[index].level;
}

/**
returns the type of the symbol at given index. returns -1 if given index is out
of bounds
*/
int symbolType(int index){
  if(index < 0 || index >= symbolTableIndex){
    return -1;
  }
  else return symbolTable[index].atribute;
}



/**
multiplication and division
*/
int term(node * current){
  factor(current);
  if(errorFlag){
    return ERROR;
  }

  int r1 = getNextOpenReg();
  regStatus[r1] = 1;

  int doMult;
  while((doMult = current->token.atribute == multsym) || current->token.atribute == slashsym){
    *current = *getNextLex(current);
    factor(current);
    if(errorFlag){
      return ERROR;
    }

    int r2 = getNextOpenReg();
    // mult or divide
    if(doMult){
      gen(mult, r1,r1, r2);
    }
    else{
      gen(divi, r1,r1,r2);
    }
  }
  regStatus[r1] = 0;

  return OK;
}
