/*
 * $Id: commands.h,v 1.1 2008-11-09 23:31:13 la7eca Exp $
 */
#ifndef _COMMANDS_H
#define _COMMANDS_H

#include "kernel/stream.h"

typedef void (*handler_func) (int argc, char *argv[], Stream* out, Stream* in);

typedef struct _command {
  const char *name;
  handler_func handler;
  const char *description;
  const char *argdef;
} command_t;


/**
 * \brief Argument definitions uses the following informal grammar
 * (single quotes denote character literals):

   <args> -> <arg> ( ',' <ws> <args> )*
 
   <arg> -> <name> ':' <ws>* <type> <ws>* <typeargs>? <default>?

   <typeargs> -> '(' <any character except closing paren> ')' 
   
   <defaul> ->  <ws>* '=' <ws>*" <string element>* " |
                <graphic character>+ | <empty>

   <ws> -> any whitespace character

   <graphic character> -> any printable character except whitespace
   
   <string element> -> any character, \ is escape char
   
 */

#ifndef PREPROCESSING
#define DEFCMD(name, description, args) \
  static const char PROGMEM __##name##_cmdid[] = #name; \
  static const char PROGMEM __##name##_descr[] = description; \
  static const char PROGMEM __##name##_args[] = args; \
  static void __##name##_handler (int, char *[], Stream*, Stream*);	\
  const command_t PROGMEM __##name##_command = \
    {__##name##_cmdid, __##name##_handler, __##name##_descr, __##name##_args}; \
  static void __##name##_handler
#endif


void cmdProcessor(Stream*, Stream*);

#endif
