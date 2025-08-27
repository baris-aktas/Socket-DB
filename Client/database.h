#ifndef DATABASE_H
#define DATABASE_H


typedef enum {
    DT_INT,
    DT_UINT,
    DT_FLOAT,
    DT_STRING,
} DataTypes;

typedef struct {
    char *AttributeName;
    DataTypes AttributeType;
} Attribute;

typedef struct {
    void **values;
} Row;

typedef struct {
    char *TableName;
    Attribute *Attributes;
    size_t AttributeCount;

    Row *Rows;
    size_t RowCount;
    size_t RowCapacity;
} Table;

typedef struct {
    char *DatabaseName;
    Table *Tables;
    size_t TableCount;
    size_t TableCapacity;
} Database;

typedef struct {
    Database *Databases;
    size_t DatabaseCount;
    size_t DatabaseCapacity;
} Schema;

#endif
