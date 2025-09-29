#ifndef ___INI_ACCESS___
#define ___INI_ACCESS___

#include <stdio.h>
#include <stdint.h>
#include "CollectionsPlus.h"

enum INIType
{
    INITypeInvalid,
    INITypeString,
    INITypeFloat
};

enum INIStreamStatus
{
    INIStreamStatusFatalFailure = -1,
    INIStreamStatusSuccess = 0,
    INIStreamStatusContinue,
    INIStreamStatusSectionHeaderParseFailed,
    INIStreamStatusPairParseFailed,
    INIStreamStatusInvalidType
};

typedef struct INIPair INIPair;
struct INIPair
{
    char *Key;
    void *Value;
    enum INIType Type; 
    INIPair *NextPair;
};

typedef struct INISection INISection;
struct INISection
{
    char *Name;
    INIPair *FirstPair;
    INISection *NextSection;
};

typedef struct INI
{
    void *Arena;

    INISection *FirstSection;
} INI;

typedef struct INIStream
{
    // These must be set
    char *IOStream;
    size_t IOStreamCount;

    //  These are used internally 
    ListGeneric LineBuffer;
    INISection *CurrentSection;
    INIPair *CurrentPair;
    size_t LineBufferRead;
} INIStream;

const INIStream INIStreamDefault = 
{
    .IOStream = NULL,
    .IOStreamCount = 0,
    .LineBuffer = ListDefault,
    .CurrentSection = NULL,
    .CurrentPair = NULL,
    .LineBufferRead = 0
};

const INI INIDefault = 
{
    .Arena = NULL,
    .FirstSection = NULL
};

int INIStreamRead(INI *INI, INIStream *Stream);
int INIStreamWrite(INI *INI, INIStream *Stream);
void INIStreamFree(INIStream *Stream);

int INIRead(INI *INI, char *file);
int INIWrite(INI *INI, char *file);
void INIFree(INI *INI);

INISection *INIFindSection(INI *INI, char *sectionName);
int INIRemoveSection(INI *INI, INISection *section);
INISection *INIAddSection(INI *INI, char *sectionName);

INIPair *INIFindPair(INISection *section, char *key);
int INIFindAndRemovePair(INISection *section, char *key);
int INIRemovePair(INISection *section, INIPair *pair);

void *INIGetValue(INIPair *pair, enum INIType type);
void *INIFindValue(INISection *section, char *key, enum INIType type);
INIPair *INIAddValue(INI *INI, INISection *section, char *key, enum INIType type, void *value);
int INISetValue(INI *INI, INIPair *pair, enum INIType type, void *value);
int INIFindAndSetValue(INI *INI, INISection *section, char *key, enum INIType type, void *value);

char *INIGetString(INIPair *pair);
char *INIFindString(INISection *section, char *key);
INIPair *INIAddString(INI *INI, INISection *section, char *key, char *string);
int INISetString(INI *INI, INIPair *pair, char *string);
int INIFindAndSetString(INI *INI, INISection *section, char *key, char *string);

int64_t *INIGetInt(INIPair *pair);
int64_t *INIFindInt(INISection *section, char *key);
INIPair *INIAddInt(INI *INI, INISection *section, char *key, int64_t integer);    
int INISetInt(INI *INI, INIPair *pair, int64_t integer);
int INIFindAndSetInt(INI* INI, INISection *section, char *key, int64_t integer);

double *INIGetFloat(INIPair *pair);
double *INIFindFloat(INISection *section, char *key);
INIPair *INIAddFloat(INI *INI, INISection *section, char *key, double number);    
int INISetFloat(INI *INI, INIPair *pair, double integer);
int INIFindAndSetFloat(INI* INI, INISection *section, char *key, double integer);

#endif