#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "database.h"
#include <stdbool.h>
#include <stddef.h>
typedef enum {
    OP_EQ, OP_NEQ, OP_GT, OP_LT, OP_GTE, OP_LTE, OP_UNKNOWN
} CompareOperator;

Table *CreateTable(const char *TableName, Attribute *Attributes, size_t AttributeCount);
void FreeTable(Table *table);
void DisplayTable(const Table *table);
bool InsertRow(Table *table, void **values);
bool PromptAndInsertRow(Table *table);
bool SaveTableToFile(const Table *table);
Table *LoadTableFromFile(const char *filename);
void ListTablesFromServer(void);
Table *LoadTableFromServer(const char *filename);
void FreeFileList(char **files, int count);
Table *PromptAndCreateTable();
void FilterAndDisplayTable(const Table *table, const char *columnName, const char *valueAsString);
bool Compare(DataTypes type, void *left, const char *rightLiteral, CompareOperator op);
CompareOperator ParseOperator(const char *op);
bool SelectQuery(Table *table, const char *columnName, const char *operator, const char *valueLiteral);
size_t DeleteRows(Table *table, const char *columnName, const char *operator, const char *valueLiteral);
size_t UpdateRows(Table *table, const char *targetColumn, const char *newValueLiteral, const char *filterColumn, const char *operator, const char *filterValueLiteral);
bool AlterAddColumn(Table *table, const char *columnName, const char *typeStr);
bool AlterDropColumn(Table *table, const char *columnName);
bool DeleteTableFile(const char *tableName);
void SendFileToServer(const char *filename);

#endif //FUNCTIONS_H
