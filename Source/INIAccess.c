#include "INIAccess.h"
#include "Assert.h"
#include <stdlib.h>
#include <Try.h>
#include <string.h>

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

TypedefList(char, ListChar);

static char *StripLeadingWhitespace(char *string)
{
    while(*string == ' ' && *string != '\0')
        string++;

    return string;
}

static void StripEndingWhitespace(char *string)
{
    while(*string != '\0')
        string++;

    string--;

    while(*string == ' ')
    {
        *string = '\0';
        string--;
    }
}

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

    return (char *)newArena + sizeof(*newArena);
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

    if(INI->FirstSection == section)
    {
        INI->FirstSection = section->NextSection;
        return 0;
    }

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

    void *newElement;
    TryNotNull(newElement = INIAllocate(INI, elementSize), NULL);
    *(void **)((char *)newElement + nextElementOffset) = NULL;

    void *element = *firstElement;
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

    char *storedSectionName;
    TryNotNull(storedSectionName = INIAllocate(INI, strlen(sectionName) + 1), NULL);
    strcpy(storedSectionName, sectionName);

    INISection *newSection;
    TryNotNull(newSection = INIAddLinkedListElement(INI, (void **)&INI->FirstSection, sizeof(*newSection), offsetof(INISection, NextSection)), NULL);
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

    char *storedKeyName;
    TryNotNull(storedKeyName = INIAllocate(INI, strlen(key) + 1), NULL);
    strcpy(storedKeyName, key);

    INIPair *newPair;
    TryNotNull(newPair = INIAddLinkedListElement(INI, (void **)&section->FirstPair, sizeof(*newPair), offsetof(INIPair, NextPair)), NULL);
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

    if(section->FirstPair == pair)
    {
        section->FirstPair = pair->NextPair;
        return 0;
    }

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
    TryNotNull(pair = INIFindPair(section, key), -1);
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
    TryNotNull(pair = INIAddPair(INI, section, key), NULL);
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
            TryNotNull(storedValue = INIAllocate(INI, strlen((char *)value) + 1), -1);
            strcpy(storedValue, value);
            break;
        }
        case INITypeFloat:
        {
            if(pair->Type != INITypeFloat)
                TryNotNull(storedValue = INIAllocate(INI, sizeof(double)), -1);
            else
                storedValue = pair->Value;

            *(double *)storedValue = *(double *)value;
            break;
        }
        default:
            Throw(EINVAL, -1, "Invalid INIType detected while setting value");
    }

    pair->Value = storedValue;
    pair->Type = type;

    return 0;
}

int INIFindAndSetValue(INI *INI, INISection *section, char *key, enum INIType type, void *value)
{
    // Callees have asserts

    INIPair *pair;
    TryNotNull(pair = INIFindPair(section, key), -1);
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

int INIStreamRead(INI *INI, INIStream *stream)
{
    Assert(INI, EINVAL, INIStreamStatusFatalFailure);
    Assert(stream, EINVAL, INIStreamStatusFatalFailure);

    ListChar *lineBuffer = (ListChar *)&stream->LineBuffer;

    while(1)
    {
        if(lineBuffer->Count > 0 && (lineBuffer->V[lineBuffer->Count - 1] == '\n' || stream->IOStreamCount == 0))
        {
            const char terminator = '\0';
            Try(ListAdd(lineBuffer, &terminator), INIStreamStatusFatalFailure);
            char *line = StripLeadingWhitespace(lineBuffer->V);
            
            if(*line == '[')
            {
                char *sectionName = line + 1;
                while(*line != ']')
                {
                    if(*line == '\0' || *line == '\n')
                        goto FailSection;

                    line++;
                }

                *line = '\0';

                if(*StripLeadingWhitespace(line + 1) != '\n')
                    goto FailSection;

                TryNotNull(INIAddSection(INI, sectionName), INIStreamStatusFatalFailure, 
                    if(errno == EINVAL)
                        goto FailSection;
                );
                ListClear(lineBuffer);
                continue;
                    
                FailSection:
                ListClear(lineBuffer);
                Throw(EINVAL, INIStreamStatusSectionHeaderParseFailed, "INI stream failed to parse section header");
            }
            else if(*line == '\n' || *line == '#')
            {
                ListClear(lineBuffer);
                continue;
            }
            else
            {
                if(INI->FirstSection == NULL)
                    goto FailPair;

                char *key = StripLeadingWhitespace(line);
                while(*line != '=')
                {
                    if(*line == '\n' || *line == '\0')
                        goto FailPair;

                    line++;
                }

                *line = '\0';
                StripEndingWhitespace(key);

                line++;
                char *value = StripLeadingWhitespace(line);

                while(*line != '\n' && *line != '\0')
                    line++;

                *line = '\0';
                StripEndingWhitespace(value);

                INISection *section = INI->FirstSection;
                while(section != NULL && section->NextSection != NULL)
                    section = section->NextSection;

                INIPair *pair;
                TryNotNull(pair = INIAddPair(INI, section, key), INIStreamStatusFatalFailure,
                    if(errno == EINVAL)
                        goto FailPair;
                );

                switch(value[0])
                {
                    case '"':
                    {
                        value++;
                        size_t valueLength = strlen(value);
                        if(value[valueLength - 1] != '"')
                            goto FailPair;
                        value[valueLength - 1] = '\0';
                        Try(INISetString(INI, pair, value), INIStreamStatusFatalFailure);
                        break;
                    }
                    default:
                    {
                        double valueFloat = strtod(value, NULL);
                        Try(INISetFloat(INI, pair, valueFloat), INIStreamStatusFatalFailure);
                        break;
                    }
                }

                ListClear(lineBuffer);
                continue;

                FailPair:
                ListClear(lineBuffer);
                Throw(EINVAL, INIStreamStatusPairParseFailed, "INI stream failed to parse pair");
            }
        }

        if(stream->IOStreamCount == 0)
            break;

        char curChar = *stream->IOStream;
        Try(ListAdd(lineBuffer, &curChar), INIStreamStatusFatalFailure);

        stream->IOStream++;
        stream->IOStreamCount--;
    }

    return INIStreamStatusSuccess;
}

int INIStreamWrite(INI *INI, INIStream *stream)
{
    Assert(INI, EINVAL, INIStreamStatusFatalFailure);
    Assert(stream, EINVAL, INIStreamStatusFatalFailure);

    ListChar *lineBuffer = (ListChar *)&stream->LineBuffer;
    
    while(1)
    {
        for(; stream->LineBufferRead < lineBuffer->Count; stream->LineBufferRead++)
        {
            if(stream->IOStreamCount == 0)
                return INIStreamStatusContinue;

            *stream->IOStream = lineBuffer->V[stream->LineBufferRead];

            stream->IOStream++;
            stream->IOStreamCount--;
        }

        ListClear(lineBuffer);

        if(stream->CurrentPair == NULL)
        {
            if(stream->CurrentSection == NULL)
                stream->CurrentSection = INI->FirstSection;
            else
                stream->CurrentSection = stream->CurrentSection->NextSection;

            if(stream->CurrentSection == NULL)
                return INIStreamStatusSuccess;

            // Handle writing section header

            const char headerBegin = '[', headerEnd = ']';

            Try(ListAdd(lineBuffer, &headerBegin), INIStreamStatusFatalFailure);
            Try(ListAddRange(lineBuffer, stream->CurrentSection->Name, strlen(stream->CurrentSection->Name)), INIStreamStatusFatalFailure);
            Try(ListAdd(lineBuffer, &headerEnd), INIStreamStatusFatalFailure);

            stream->CurrentPair = stream->CurrentSection->FirstPair;
        }
        else
        {
            // Handle writing pair info

            const char separator[] = " = ";

            Try(ListAddRange(lineBuffer, stream->CurrentPair->Key, strlen(stream->CurrentPair->Key)), INIStreamStatusFatalFailure);
            Try(ListAddRange(lineBuffer, separator, sizeof(separator) - 1), INIStreamStatusFatalFailure);

            switch(stream->CurrentPair->Type)
            {
                case INITypeString:
                {
                    char quote = '"';
                    Try(ListAdd(lineBuffer, &quote), INIStreamStatusFatalFailure);
                    Try(ListAddRange(lineBuffer, stream->CurrentPair->Value, strlen(stream->CurrentPair->Value)), INIStreamStatusFatalFailure);
                    Try(ListAdd(lineBuffer, &quote), INIStreamStatusFatalFailure);
                    break;
                }
                case INITypeFloat:
                {
                    char buffer[256];
                    snprintf(buffer, sizeof(buffer), "%g", *(double *)stream->CurrentPair->Value);
                    Try(ListAddRange(lineBuffer, buffer, strlen(buffer)), INIStreamStatusFatalFailure);
                    break;
                }
                case INITypeInvalid:
                {
                    stream->CurrentPair = stream->CurrentPair->NextPair;
                    stream->LineBufferRead = 0;
                    return INIStreamStatusInvalidType;
                    break;
                }
            }

            stream->CurrentPair = stream->CurrentPair->NextPair;
        }

        const char newline = '\n';
        Try(ListAdd(lineBuffer, &newline), INIStreamStatusFatalFailure);
        stream->LineBufferRead = 0;
    }
}

void INIStreamFree(INIStream *stream)
{
    ListFree(&stream->LineBuffer);
}

int INIRead(INI *INI, char *fileName)
{
    Assert(INI, EINVAL, INIStreamStatusFatalFailure);
    Assert(fileName, EINVAL, INIStreamStatusFatalFailure);

    int retVal = INIStreamStatusSuccess;

    FILE *file = fopen(fileName, "r");
    Assert(file != NULL, errno, INIStreamStatusFatalFailure);

    char buffer[256];
    INIStream stream = INIStreamDefault;
    size_t sectionParseFailCount = 0;

    while(1)
    {
        size_t read = fread(buffer, sizeof(char), sizeof(buffer), file);
        stream.IOStream = buffer;
        stream.IOStreamCount = read; 

        while(1)
        {
            int code = INIStreamRead(INI, &stream);
            switch(code)
            {
                case INIStreamStatusFatalFailure:
                    retVal = INIStreamStatusFatalFailure;
                    goto End;
                case INIStreamStatusSuccess:
                    goto Continue;
                case INIStreamStatusPairParseFailed:
                    continue;
                case INIStreamStatusSectionHeaderParseFailed:
                {
                    char fallbackSectionName[64];
                    snprintf(fallbackSectionName, sizeof(fallbackSectionName), "ParseFailed_%zu", sectionParseFailCount);
                    INIAddSection(INI, fallbackSectionName);
                    sectionParseFailCount++;
                    continue;
                }
            }
        }

        Continue:

        if(feof(file))
            break;
        AssertDo(!ferror(file), ferror(file), retVal = INIStreamStatusFatalFailure; goto End;);
    }

    End:
    fclose(file);
    INIStreamFree(&stream);
    return retVal;
}

int INIWrite(INI *INI, char *fileName)
{
    Assert(INI, EINVAL, INIStreamStatusFatalFailure);
    Assert(fileName, EINVAL, INIStreamStatusFatalFailure);

    int retVal = INIStreamStatusSuccess;

    FILE *file = fopen(fileName, "w");
    Assert(file, errno, INIStreamStatusFatalFailure);

    char buffer[256];
    INIStream stream = INIStreamDefault;

    while (1)
    {
        stream.IOStream = buffer;
        stream.IOStreamCount = sizeof(buffer);

        while (1)
        {
            int code = INIStreamWrite(INI, &stream);

            switch(code)
            {
                case INIStreamStatusFatalFailure:
                    retVal = INIStreamStatusFatalFailure;
                    goto End;
                case INIStreamStatusSuccess:
                {
                    fwrite(buffer, sizeof(char), sizeof(buffer) - stream.IOStreamCount, file);
                    AssertDo(!ferror(file), ferror(file), retVal = INIStreamStatusFatalFailure;);
                    goto End;
                }
                case INIStreamStatusContinue:
                    goto Continue;
                case INIStreamStatusInvalidType:
                    continue;
            }
        }

        Continue:
        
        fwrite(buffer, sizeof(char), sizeof(buffer), file);
        AssertDo(!ferror(file), ferror(file), retVal = INIStreamStatusFatalFailure; goto End;);
    }

    End:
    fclose(file);
    INIStreamFree(&stream);
    return retVal;
}

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