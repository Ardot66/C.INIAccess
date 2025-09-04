#include "INIAccess.h"
#include "Try.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void *UpdatePointer(void *pointer, void *old, void *new)
{
    if(pointer == NULL)
        return pointer;

    ptrdiff_t offset = (char *)pointer - (char *)old;
    return (char *)new + offset;
}

static void *INIAllocate(INI *INI, size_t size)
{
    assert(INI->ArenaUsed < INI->ArenaSize);

    if(INI->ArenaSize - INI->ArenaUsed < size)
    {
        size_t newSize = INI->ArenaSize;
        while (newSize - INI->ArenaUsed < size)
            newSize *= 2;
        
        void *oldPtr = INI->Arena;
        void *temp = realloc(INI->Arena, newSize);
        if(temp == NULL)
            Throw(errno, NULL, "");

        INI->Arena = temp;
        INI->ArenaSize = newSize;

        INI->FirstSection = UpdatePointer(INI->FirstSection, oldPtr, INI->Arena);
        INISection *section = INI->FirstSection;

        while(section != NULL)
        {
            section->FirstPair = UpdatePointer(section->FirstPair, oldPtr, INI->Arena);
            INIPair *pair = section->FirstPair;

            while(pair != NULL)
            {
                pair->Key = UpdatePointer(pair->Key, oldPtr, INI->Arena);
                pair->Value = UpdatePointer(pair->Value, oldPtr, INI->Arena);
                pair->NextPair = UpdatePointer(pair->NextPair, oldPtr, INI->Arena);

                pair = pair->NextPair;
            }

            section->NextSection = UpdatePointer(section->NextSection, oldPtr, INI->Arena);
            section = section->NextSection;
        }
    }

    INI->ArenaUsed += size;
    return (char *)INI->Arena + INI->ArenaUsed - size;
}

void INIStreamRead(INI *INI, INIStream *Stream);
void INIStreamWrite(INI *INI, INIStream *Stream);
void INIStreamFree(INIStream *Stream);

int INICreate(INI *INI)
{
    INI->Arena = malloc(1024);
    if(INI->Arena == NULL)
        Throw(errno, -1, "");

    INI->ArenaSize = 0;
    INI->ArenaUsed = 0;
    INI->FirstSection = NULL;

    return 0;
}

void INIRead(INI *INI, char *file);
void INIWrite(INI *INI, char *file);
void INIFree(INI *INI);

INISection *INIFindSection(char *sectionName);
void INIRemoveSection(INISection *section);

INISection *INIAddSection(INI *INI, char *sectionName)
{
    if(INIFindSection(sectionName) != NULL)
        Throw(EINVAL, NULL, "Cannot add a section with a name that is already in use by another section");

    // Need to allocate first otherwise pointers may be invalidated
    size_t sectionNameLength = strlen(sectionName);
    void *allocation = INIAllocate(INI, sectionNameLength + 1 + sizeof(INISection));

    char *storedSectionName = allocation;
    strcpy(storedSectionName, sectionName);

    INISection *newSection = (char *)allocation + sectionNameLength + 1;

    INISection *section = INI->FirstSection;
    while(section != NULL && section->NextSection != NULL)
        section = section->NextSection;

    section->NextSection = newSection;
    newSection->Name = storedSectionName;
    newSection->FirstPair = NULL;
    newSection->NextSection = NULL;

    return newSection;
}

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
