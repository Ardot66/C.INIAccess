#ifndef ___INI_ACCESS___
#define ___INI_ACCESS___

#include <stdio.h>
#include <stdint.h>
#include "CollectionsPlus.h"

typedef struct INIStream
{
    // These must be set
    char *IOStream;
    size_t IOStreamCount;

    //  These are used internally 
    ListGeneric LineBuffer;
} INIStream;

typedef struct INIPair INIPair;
struct INIPair
{
    char *Key;
    void *Value;
    INIPair *NextPair;
};

typedef struct INISection INISection;
struct INISection
{
    char *Name;
    size_t Count;
    INIPair *FirstPair;
    INISection *NextSection;
};

typedef struct INI
{
    void *Arena;
    size_t ArenaSize;
    size_t ArenaUsed;

    INISection *FirstSection;
} INI;

void INIStreamRead(INI *INI, INIStream *Stream);
void INIStreamWrite(INI *INI, INIStream *Stream);
void INIStreamFree(INIStream *Stream);

int INICreate(INI *INI);
void INIRead(INI *INI, char *file);
void INIWrite(INI *INI, char *file);
void INIFree(INI *INI);

INISection *INIFindSection(char *sectionName);
void INIRemoveSection(INISection *section);
INISection *INIAddSection(INI *INI, char *sectionName);

INIPair *INIFindPair(INISection *section, char *key);
void INIFindAndRemovePair(INISection *section, char *key);
void INIRemovePair(INISection *section, INIPair *pair);

char *INIGetString(INIPair *pair);
char *INIFindString(INISection *section, char *key);
char *INIAddString(INI *INI, INISection *section, char *key, char *string);    

uint64_t *INIGetInt(INIPair *pair);
uint64_t *INIFindInt(INISection *section, char *key);
uint64_t *INIAddInt(INI *INI, INISection *section, char *key, uint64_t integer);    

double *INIGetFloat(INIPair *pair);
double *INIFindFloat(INISection *section, char *key);
double *INIAddFloat(INI *INI, INISection *section, char *key, double number);    

#endif