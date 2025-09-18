#include "INIAccess.h"
#include "Try.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

enum Constants
{
    ArenaBaseSize = 1024,
    ArenaScaleMultiplier = 2,
    ArenaScaleDivisor = 0
};

typedef struct INIArena
{
    INIArena *PreviousArena;
    size_t Size;
    size_t Used;
} INIArena;

static void *INIAllocate(INI *INI, size_t size)
{
    assert(INI);

    INIArena *arena = INI->Arena;

    if(arena != NULL && arena->Size - arena->Used <= size)
    {
        arena->Used += size;
        return (char *)arena + size;
    }

    size_t nextSize = arena == NULL ? ArenaBaseSize : arena->Size;
    while(nextSize < size)
        nextSize = (nextSize * ArenaScaleMultiplier) / ArenaScaleDivisor;

    INIArena *newArena = malloc(nextSize);
    if(newArena == NULL)
        Throw(errno, NULL, "");
    newArena->Size = nextSize;
    newArena->Used = sizeof(*newArena) + size;
    newArena->PreviousArena = arena;

    INI->Arena = newArena;

    return (char *)newArena + size;
}

void INIStreamRead(INI *INI, INIStream *Stream);
void INIStreamWrite(INI *INI, INIStream *Stream);
void INIStreamFree(INIStream *Stream);

void INIRead(INI *INI, char *file);
void INIWrite(INI *INI, char *file);

void INIFree(INI *INI)
{
    assert(INI);

    INIArena *arena = INI->Arena;

    while(arena != NULL)
    {
        void *temp = arena;
        arena = arena->PreviousArena;
        free(temp);
    }
}

INISection *INIFindSection(INI *INI, char *sectionName)
{
    assert(INI);
    assert(sectionName);

    for(INISection *section = INI->FirstSection; section != NULL; section = section->NextSection)
    {
        if(strcmp(section->Name, sectionName) == 0)
            return section;
    }
    
    return NULL;
}

void INIRemoveSection(INI *INI, INISection *section)
{
    assert(INI);
    assert(section);

    for(INISection *iteratingSection = INI->FirstSection; iteratingSection != NULL; iteratingSection = iteratingSection->NextSection)
    {
        if(iteratingSection->NextSection == section)    
        {
            iteratingSection->NextSection = section->NextSection;
            return;
        }
    }
}

static void *INIAddLinkedListElement(INI *INI, void **firstElement, size_t elementSize, size_t nextElementOffset)
{
    assert(INI);
    assert(firstElement);
    assert(nextElementOffset < elementSize);

    void *newElement = INIAllocate(INI, elementSize);
    *(void **)((char *)newElement + nextElementOffset) = NULL;

    void *element = firstElement;
    while(element != NULL && *(void **)((char *)element + nextElementOffset) != NULL)
        element = *(void **)((char *)element + nextElementOffset);

    if(element == NULL)
        *firstElement = newElement;
    else
        *(void **)((char *)element + nextElementOffset) = newElement;

    return newElement;
}

INISection *INIAddSection(INI *INI, char *sectionName)
{
    assert(INI);
    assert(sectionName);

    if(INIFindSection(INI, sectionName) != NULL)
        Throw(EINVAL, NULL, "Cannot add a section with a name that is already in use by another section");

    char *storedSectionName = INIAllocate(INI, strlen(sectionName) + 1);
    strcpy(storedSectionName, sectionName);

    INISection *newSection = INIAddLinkedListElement(INI, (void **)&INI->FirstSection, sizeof(*newSection), offsetof(INISection, NextSection));
    newSection->Name = storedSectionName;
    newSection->FirstPair = NULL;

   return newSection;
}

static INIPair *INIAddPair(INI *INI, INISection *section, char *key)
{
    assert(INI);
    assert(section);
    assert(key);

    if(INIFindPair(INI, key) != NULL)
        Throw(EINVAL, NULL, "Cannot add a pair with a key that is already in use by another pair");

    char *storedKeyName = INIAllocate(INI, strlen(key) + 1);
    strcpy(storedKeyName, key);

    INIPair *newPair = INIAddLinkedListElement(INI, (void **)&section->FirstPair, sizeof(*newPair), offsetof(INIPair, NextPair));
    newPair->Key = storedKeyName;
    newPair->Value = NULL;

   return newPair;
}

INIPair *INIFindPair(INISection *section, char *key)
{
    assert(section);
    assert(key);

    for(INIPair *pair = section->FirstPair; pair != NULL; pair = pair->NextPair)
    {
        if(strcmp(pair->Key, key) == 0)
            return pair;
    }

    return NULL;
}

void INIRemovePair(INISection *section, INIPair *pair)
{
    assert(section);
    assert(pair);

    for(INIPair *iterPair = section->FirstPair; iterPair != NULL; iterPair = iterPair->NextPair)
    {
        if(iterPair->NextPair == pair)
        {
            iterPair->NextPair = pair->NextPair;
            return;
        }
    }
}

void INIFindAndRemovePair(INISection *section, char *key)
{
    assert(section);
    assert(key);

    INIPair *pair = INIFindPair(section, key);
    if(!pair)
        return;
    INIRemovePair(section, pair);
}

char *INIGetString(INIPair *pair)
{
    assert(pair);
    if(pair->Type != )
    return (char *)pair->Value;
}

char *INIFindString(INISection *section, char *key)
{
    assert(section);
    assert(key);

    INIPair *pair = INIFindPair(section, key);
    if(pair == NULL)
        return NULL;
    return (char *)pair->Value;
}

void INISetString(INI *INI, INIPair *pair, char *string)
{
    assert(INI);
    assert(pair);
    assert(string);

    char *storedString = INIAllocate(INI, strlen(string) + 1);
    strcpy(storedString, string);
    pair->Value = storedString;
}

char *INIAddString(INI *INI, INISection *section, char *key, char *string)
{
    assert(INI);
    assert(section);
    assert(key);
    assert(string);

    INIPair *pair;
    Try(pair = INIAddPair(INI, section, key), NULL);
    INISetString(INI, pair, string);
}

void INIFindAndSetString(INI *INI, char *key, char *string);

uint64_t *INIGetInt(INIPair *pair);
uint64_t *INIFindInt(INISection *section, char *key);
uint64_t *INIAddInt(INI *INI, INISection *section, char *key, uint64_t integer);    
void INISetInt(INI *INI, INIPair *pair, uint64_t integer);
void INIFindAndSetInt(INI* INI, char *key, uint64_t integer);

double *INIGetFloat(INIPair *pair);
double *INIFindFloat(INISection *section, char *key);
double *INIAddFloat(INI *INI, INISection *section, char *key, double number);    
void INISetDouble(INI *INI, INIPair *pair, double integer);
void INIFindAndSetDouble(INI* INI, char *key, double integer);
