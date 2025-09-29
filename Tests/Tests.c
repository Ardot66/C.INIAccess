#include <string.h>
#include "INIAccess.h"
#include "TestingUtilities.h"
#include "Try.h"

const char *INIString = 
"[Section]\n"
"Key = \"Value\"\n"
"Number = 1\n";

void TestINIValidity(INI *INI)
{
    TEST(INI->FirstSection, !=, NULL);
    INISection *firstSection = INI->FirstSection;
    TEST(strcmp(firstSection->Name, "Section"), ==, 0);

    INIPair *firstPair = firstSection->FirstPair;
    TEST(firstPair, !=, NULL);
    TEST(strcmp(firstPair->Key, "Key"), ==, 0);
    TEST(strcmp(INIGetString(firstPair), "Value"), ==, 0)

    INIPair *secondPair = firstPair->NextPair;
    TEST(secondPair, !=, NULL);
    TEST(strcmp(secondPair->Key, "Number"), ==, 0);
    TEST(*INIGetFloat(secondPair), ==, 1);
}

int main()
{
    INI INI = INIDefault;

    int code = INIRead(&INI, "Tests/TestINI.ini");
    TEST(code, ==, 0, ErrorCurrentPrint(););

    TestINIValidity(&INI);
    INIFree(&INI);

    INISection *section = INI.FirstSection;
    INIPair *pair;
    TEST((pair = INIFindPair(section, "Key")), !=, NULL, ErrorCurrentPrint(););
    TEST(strcmp(pair->Key, "Key"), ==, 0);

    INIRemovePair(section, pair);
    TEST(section->FirstPair, ==, pair->NextPair);

    INI = INIDefault;
    TEST((section = INIAddSection(&INI, "Section")), !=, NULL, ErrorCurrentPrint(););
    TEST(INIAddString(&INI, section, "Key", "Value"), !=, NULL, ErrorCurrentPrint(););
    TEST(INIAddFloat(&INI, section, "Number", 1), !=, NULL, ErrorCurrentPrint(););
    
    TestINIValidity(&INI);
    
    code = INIWrite(&INI, "Bin/OutINI.ini");
    TEST(code, ==, 0, ErrorCurrentPrint(););
    INIFree(&INI);

    FILE *file = fopen("Bin/OutINI.ini", "r");
    char buffer[256];
    size_t read = fread(buffer, 1, sizeof(buffer), file);
    buffer[read] = '\0';
    TEST(strcmp(INIString, buffer), ==, 0);
    fclose(file);

    TestsEnd();
}