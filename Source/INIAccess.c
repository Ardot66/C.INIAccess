#include "INIAccess.h"
#include "Assert.h"
#include <stdlib.h>
#include <Try.h>
#include <string.h>
#include <pthread.h>

const char 
    *PairTypeMismatchMessage = "Type mismatch detected while reading data from INI pair",
    *PairNotFoundMessage = "INI pair not found while finding pair to read";

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

char *INIGetString(INIPair *pair)
{
    Assert(pair, EINVAL, NULL);
    AssertMsg(pair->Type == INITypeString, EINVAL, NULL, PairTypeMismatchMessage);
    return (char *)pair->Value;
}

char *INIFindString(INISection *section, char *key)
{
    INIPair *pair = INIFindPair(section, key);
    return pair ? INIGetString(pair) : NULL;
}

INIPair *INIAddString(INI *INI, INISection *section, char *key, char *string)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIAddPair(INI, section, key), NULL);
    Try(INISetString(INI, pair, string), NULL);

    return pair;
}

int INISetString(INI *INI, INIPair *pair, char *string)
{
    Assert(pair, EINVAL, -1);
    Assert(string, EINVAL, -1);

    // Could optimize to not allocate extra for smaller strings

    char *storedString;
    Try(storedString = INIAllocate(INI, strlen(string) + 1), -1);

    strcpy(storedString, string);
    pair->Value = storedString;
    pair->Type = INITypeString;

    return 0;
}

int INIFindAndSetString(INI *INI, INISection *section, char *key, char *string)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIFindPair(section, key), -1);
    Try(INISetString(INI, pair, string), -1);

    return 0;
}

int64_t *INIGetInt(INIPair *pair)
{
    Assert(pair, EINVAL, NULL);
    AssertMsg(pair->Type == INITypeInt, EINVAL, NULL, PairTypeMismatchMessage);
    return (int64_t *)pair->Value;
}

int64_t *INIFindInt(INISection *section, char *key)
{
    INIPair *pair = INIFindPair(section, key);
    return pair ? INIGetInt(pair) : NULL;
}

INIPair *INIAddInt(INI *INI, INISection *section, char *key, int64_t integer)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIAddPair(INI, section, key), NULL);
    Try(INISetInt(INI, pair, integer), NULL);

    return pair;
}

int INISetInt(INI *INI, INIPair *pair, int64_t integer)
{
    Assert(pair, EINVAL, -1);

    int64_t *storedInt;
    if(pair->Type != INITypeInt)
    {
        Try(storedInt = INIAllocate(INI, sizeof(*storedInt)), -1);
        pair->Value = storedInt;
        pair->Type = INITypeInt;
    }
    else
        storedInt = pair->Value;

    *storedInt = integer;
    return 0;
}

int INIFindAndSetInt(INI* INI, INISection *section, char *key, int64_t integer)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIFindPair(section, key), -1);
    Try(INISetInt(INI, pair, integer), -1);

    return 0;
}

double *INIGetFloat(INIPair *pair)
{
    Assert(pair, EINVAL, NULL);
    AssertMsg(pair->Type == INITypeFloat, EINVAL, NULL, PairTypeMismatchMessage);
    return (double *)pair->Value;
}

double *INIFindFloat(INISection *section, char *key)
{
    INIPair *pair = INIFindPair(section, key);
    return pair ? INIGetFloat(pair) : NULL;
}

INIPair *INIAddFloat(INI *INI, INISection *section, char *key, double number)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIAddPair(INI, section, key), NULL);
    Try(INISetFloat(INI, pair, number), NULL);

    return pair;
}

int INISetFloat(INI *INI, INIPair *pair, double number)
{
    Assert(pair, EINVAL, -1);

    int64_t *storedFloat;
    if(pair->Type != INITypeInt)
    {
        Try(storedFloat = INIAllocate(INI, sizeof(*storedFloat)), -1);
        pair->Value = storedFloat;
        pair->Type = INITypeInt;
    }
    else
        storedFloat = pair->Value;

    *storedFloat = number;
    return 0;
}

int INIFindAndSetFloat(INI* INI, INISection *section, char *key, double number)
{
    // Callees have asserts

    INIPair *pair;
    Try(pair = INIFindPair(section, key), -1);
    Try(INISetFloat(INI, pair, number), -1);

    return 0;
}
