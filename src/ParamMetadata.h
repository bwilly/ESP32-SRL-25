#pragma once
// #include <Arduino.h>

// // Declaration for paramList
// extern std::vector<ParamMetadata> paramList;

// In ParamMetadata.h
struct ParamMetadata
{
    std::string name;
    std::string spiffsPath;
    enum Type
    {
        STRING,
        NUMBER,
        BOOLEAN,
        HEX_ARRAY,
        STRING_ARRAY
    } type;
};
