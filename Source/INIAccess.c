#include "INIAccess.h"
#include "Assert.h"
#include <stdlib.h>
#include <Try.h>
#include <string.h>
#include <pthread.h>

const char *PairTypeMismatchMessage = "Type mismatch detected while reading data from INI pair";

enum Constants
{
    ArenaBaseSize = 1024,
    ArenaScaleMultiplier = 2,
    ArenaScaleDivisor = 1
};

typedef struct INIArena INIArena; 
struct INIArena
{
    INIArena *PreviousArena;
    size_t Size;
    size_t Used;
} ;

static void *INIAllocate(INI *INI, size_t size)
{
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
    Assert(newArena != NULL, errno, NULL);

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
    Assert(INI, EINVAL, );

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
    Assert(INI, EINVAL, NULL);
    Assert(sectionName, EINVAL, NULL);

    for(INISection *section = INI->FirstSection; section != NULL; section = section->NextSection)
    {
        if(strcmp(section->Name, sectionName) == 0)
            return section;
    }
    
    return NULL;
}

int INIRemoveSection(INI *INI, INISection *section)
{
    Assert(INI, EINVAL, -1);
    Assert(section, EINVAL, -1);

    for(INISection *iteratingSection = INI->FirstSection; iteratingSection != NULL; iteratingSection = iteratingSection->NextSection)
    {
        if(iteratingSection->NextSection == section)    
        {
            iteratingSection->NextSection = section->NextSection;
            return 0;
        }
    }

    return 0;
}

static void *INIAddLinkedListElement(INI *INI, void **firstElement, size_t elementSize, size_t nextElementOffset)
{
    Assert(INI, EINVAL, NULL);
    Assert(firstElement, EINVAL, NULL);
    Assert(nextElementOffset < elementSize, EINVAL, NULL);

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
    Assert(INI, EINVAL, NULL);
    Assert(sectionName, EINVAL, NULL);
    AssertMsg(INIFindSection(INI, sectionName) == NULL, EINVAL, NULL, "Cannot add a section with a name that is already in use by another section");

    char *storedSectionName = INIAllocate(INI, strlen(sectionName) + 1);
    strcpy(storedSectionName, sectionName);

    INISection *newSection = INIAddLinkedListElement(INI, (void **)&INI->FirstSection, sizeof(*newSection), offsetof(INISection, NextSection));
    newSection->Name = storedSectionName;
    newSection->FirstPair = NULL;

   return newSection;
}

static INIPair *INIAddPair(INI *INI, INISection *section, char *key)
{
    Assert(INI, EINVAL, NULL);
    Assert(section, EINVAL, NULL);
    Assert(key, EINVAL, NULL);
    AssertMsg(INIFindPair(section, key) == NULL, EINVAL, NULL, "Cannot add a pair with a key that is already in use by another pair");

    char *storedKeyName = INIAllocate(INI, strlen(key) + 1);
    strcpy(storedKeyName, key);

    INIPair *newPair = INIAddLinkedListElement(INI, (void **)&section->FirstPair, sizeof(*newPair), offsetof(INIPair, NextPair));
    newPair->Key = storedKeyName;
    newPair->Value = NULL;
    newPair->Type = INITypeInvalid;

   return newPair;
}

INIPair *INIFindPair(INISection *section, char *key)
{
    Assert(section, EINVAL, NULL);
    Assert(key, EINVAL, NULL);

    for(INIPair *pair = section->FirstPair; pair != NULL; pair = pair->NextPair)
    {
        if(strcmp(pair->Key, key) == 0)
            return pair;
    }

    return NULL;
}

int INIRemovePair(INISection *section, INIPair *pair)
{
    Assert(section, EINVAL, -1);
    Assert(pair, EINVAL, -1);

    for(INIPair *iterPair = section->FirstPair; iterPair != NULL; iterPair = iterPair->NextPair)
    {
        if(iterPair->NextPair == pair)
        {
            iterPair->NextPair = pair->NextPair;
            return 0;
        }
    }

    return 0;
}

int INIFindAndRemovePair(INISection *section, char *key)
{
    INIPair *pair;
    Try(pair = INIFindPair(section, key), -1);
    Try(INIRemovePair(section, pair), -1);

    return 0;
}

void *INIGetValue(INIPair *pair, enum INIType type)
{
    Assert(pair, EINVAL, NULL);
    AssertMsg(pair->Type == type, EINVAL, NULL, PairTypeMismatchMessage);
    return pair->Value;
}

void *INIFindValue(INISection *section, char *key, enum INIType type)
{
    INIPair *pair = INIFindPair(section, key);
    return pair ? INIGetValue(pair, type) : NULL;
}

INIPair *INIAddValue(INI *INI, INISection *section, char *key, enum INIType type, void *value)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIAddPair(INI, section, key), NULL);
    Try(INISetValue(INI, pair, type, value), NULL);

    return pair;
}

int INISetValue(INI *INI, INIPair *pair, enum INIType type, void *value)
{
    Assert(pair, EINVAL, -1);
    Assert(value, EINVAL, -1);

    void *storedValue;

    switch(type)
    {
        case INITypeString:
        {
            // Could optimize to not allocate extra for smaller strings
            Try(storedValue = INIAllocate(INI, strlen((char *)value) + 1), -1);
            strcpy(storedValue, value);
            break;
        }
        case INITypeInt:
        {
            if(pair->Type != INITypeInt)
                Try(storedValue = INIAllocate(INI, sizeof(int64_t)), -1);
            else
                storedValue = pair->Value;

            *(int64_t *)storedValue = *(int64_t *)value;
        }
        case INITypeFloat:
        {
            if(pair->Type != INITypeFloat)
                Try(storedValue = INIAllocate(INI, sizeof(double)), -1);
            else
                storedValue = pair->Value;

            *(double *)storedValue = *(double *)value;
        }
    }

    pair->Value = storedValue;
    pair->Type = type;

    return 0;
}

int INIFindAndSetValue(INI *INI, INISection *section, char *key, enum INIType type, void *value)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIFindPair(section, key), -1);
    Try(INISetValue(INI, pair, type, value), -1);

    return 0;
}

// Type safe functions for convenience

char *INIGetString(INIPair *pair)
{
    return INIGetValue(pair, INITypeString);
}

char *INIFindString(INISection *section, char *key)
{
    return INIFindValue(section, key, INITypeString);
}

INIPair *INIAddString(INI *INI, INISection *section, char *key, char *string)
{
    return INIAddValue(INI, section, key, INITypeString, string);
}

int INISetString(INI *INI, INIPair *pair, char *string)
{
    return INISetValue(INI, pair, INITypeString, string);
}

int INIFindAndSetString(INI *INI, INISection *section, char *key, char *string)
{
    return INIFindAndSetValue(INI, section, key, INITypeString, string);
}

int64_t *INIGetInt(INIPair *pair)
{
    return INIGetValue(pair, INITypeInt);
}

int64_t *INIFindInt(INISection *section, char *key)
{
    return INIFindValue(section, key, INITypeInt);
}

INIPair *INIAddInt(INI *INI, INISection *section, char *key, int64_t integer)
{
    return INIAddValue(INI, section, key, INITypeInt, &integer);
}

int INISetInt(INI *INI, INIPair *pair, int64_t integer)
{
    return INISetValue(INI, pair, INITypeInt, &integer);
}

int INIFindAndSetInt(INI* INI, INISection *section, char *key, int64_t integer)
{
    return INIFindAndSetValue(INI, section, key, INITypeInt, &integer);
}

double *INIGetFloat(INIPair *pair)
{
    return INIGetValue(pair, INITypeFloat);
}

double *INIFindFloat(INISection *section, char *key)
{
    return INIFindValue(section, key, INITypeFloat);
}

INIPair *INIAddFloat(INI *INI, INISection *section, char *key, double number)
{
    return INIAddValue(INI, section, key, INITypeFloat, &number);
}

int INISetFloat(INI *INI, INIPair *pair, double number)
{
    return INISetValue(INI, pair, INITypeFloat, &number);
}

int INIFindAndSetFloat(INI* INI, INISection *section, char *key, double number)
{
    return INIFindAndSetValue(INI, section, key, INITypeFloat, &number);
}
