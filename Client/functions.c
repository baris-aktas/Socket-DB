#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
    #include <direct.h>
    #define MAKE_DIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h>
    #define MAKE_DIR(dir) mkdir(dir, 0777)
#endif
#include "functions.h"
#include "database.h"

#define INITIAL_ROW_CAPACITY 4

Table *CreateTable(const char *TableName, Attribute *Attributes, size_t AttributeCount) {
    Table *table = malloc(sizeof(Table));
    if (!table) return NULL;

    table->TableName = strdup(TableName);
    table->AttributeCount = AttributeCount;

    table->Attributes = malloc(sizeof(Attribute) * AttributeCount);
    for (size_t i = 0; i < AttributeCount; ++i) {
        table->Attributes[i].AttributeName = strdup(Attributes[i].AttributeName);
        table->Attributes[i].AttributeType = Attributes[i].AttributeType;
    }

    table->RowCount = 0;
    table->RowCapacity = INITIAL_ROW_CAPACITY;
    table->Rows = malloc(sizeof(Row) * table->RowCapacity);
    for (size_t i = 0; i < table->RowCapacity; ++i) {
        table->Rows[i].values = NULL;
    }

    return table;
}

void FreeTable(Table *table) {
    if (!table) return;

    free(table->TableName);

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        free(table->Attributes[i].AttributeName);
    }
    free(table->Attributes);

    for (size_t i = 0; i < table->RowCount; ++i) {
        Row *row = &table->Rows[i];
        if (row->values) {
            for (size_t j = 0; j < table->AttributeCount; ++j) {
                if (table->Attributes[j].AttributeType == DT_STRING) {
                    free(row->values[j]);
                } else {
                    free(row->values[j]);
                }
            }
            free(row->values);
        }
    }

    free(table->Rows);
    free(table);
}

void DisplayTable(const Table *table) {
    if (!table) return;

    printf("Table: %s\n", table->TableName);

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        printf("| %-15s ", table->Attributes[i].AttributeName);
    }
    printf("|\n");

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        printf("+-----------------");
    }
    printf("+\n");

    for (size_t i = 0; i < table->RowCount; ++i) {
        Row *row = &table->Rows[i];
        for (size_t j = 0; j < table->AttributeCount; ++j) {
            void *value = row->values[j];
            switch (table->Attributes[j].AttributeType) {
                case DT_INT:
                    printf("| %-15d ", *(int *)value);
                    break;
                case DT_UINT:
                    printf("| %-15u ", *(unsigned int *)value);
                    break;
                case DT_FLOAT:
                    printf("| %-15.2f ", *(float *)value);
                    break;
                case DT_STRING:
                    printf("| %-15s ", (char *)value);
                    break;
            }
        }
        printf("|\n");
    }
}

void InsertRowFromInput(Table *table) {
    if (!table) return;

    void **values = malloc(sizeof(void *) * table->AttributeCount);
    if (!values) {
        printf("Memory allocation failed.\n");
        return;
    }

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        Attribute attr = table->Attributes[i];
        char buffer[256];

        printf("Enter value for %s (%s): ", attr.AttributeName,
               attr.AttributeType == DT_INT ? "INT" :
               attr.AttributeType == DT_UINT ? "UINT" :
               attr.AttributeType == DT_FLOAT ? "FLOAT" : "STRING");

        scanf("%255s", buffer);

        switch (attr.AttributeType) {
            case DT_INT: {
                int *val = malloc(sizeof(int));
                *val = atoi(buffer);
                values[i] = val;
                break;
            }
            case DT_UINT: {
                unsigned int *val = malloc(sizeof(unsigned int));
                *val = (unsigned int)strtoul(buffer, NULL, 10);
                values[i] = val;
                break;
            }
            case DT_FLOAT: {
                float *val = malloc(sizeof(float));
                *val = strtof(buffer, NULL);
                values[i] = val;
                break;
            }
            case DT_STRING: {
                values[i] = strdup(buffer);
                break;
            }
        }
    }

    if (InsertRow(table, values)) {
        printf("Row inserted successfully.\n");
    } else {
        printf("Row insertion failed.\n");
    }

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        free(values[i]);
    }
    free(values);
}

bool InsertRow(Table *table, void **values) {
    if (!table || !values) return false;


    if (table->RowCount >= table->RowCapacity) {
        size_t newCapacity = table->RowCapacity * 2;
        Row *newRows = realloc(table->Rows, sizeof(Row) * newCapacity);
        if (!newRows) return false;
        table->Rows = newRows;
        table->RowCapacity = newCapacity;
    }

    Row *newRow = &table->Rows[table->RowCount];
    newRow->values = malloc(sizeof(void *) * table->AttributeCount);
    if (!newRow->values) return false;

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        DataTypes type = table->Attributes[i].AttributeType;

        switch (type) {
            case DT_INT: {
                int *val = malloc(sizeof(int));
                *val = *(int *)values[i];
                newRow->values[i] = val;
                break;
            }
            case DT_UINT: {
                unsigned int *val = malloc(sizeof(unsigned int));
                *val = *(unsigned int *)values[i];
                newRow->values[i] = val;
                break;
            }
            case DT_FLOAT: {
                float *val = malloc(sizeof(float));
                *val = *(float *)values[i];
                newRow->values[i] = val;
                break;
            }
            case DT_STRING: {
                newRow->values[i] = strdup((char *)values[i]);
                break;
            }
        }
    }

    table->RowCount++;
    return true;
}

bool PromptAndInsertRow(Table *table) {
    if (!table) return false;

    void **values = malloc(sizeof(void *) * table->AttributeCount);
    if (!values) return false;

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        DataTypes type = table->Attributes[i].AttributeType;
        char *attrName = table->Attributes[i].AttributeName;

        printf("Enter value for '%s': ", attrName);

        switch (type) {
            case DT_INT: {
                int *val = malloc(sizeof(int));
                if (!val) return false;
                scanf("%d", val);
                values[i] = val;
                break;
            }
            case DT_UINT: {
                unsigned int *val = malloc(sizeof(unsigned int));
                if (!val) return false;
                scanf("%u", val);
                values[i] = val;
                break;
            }
            case DT_FLOAT: {
                float *val = malloc(sizeof(float));
                if (!val) return false;
                scanf("%f", val);
                values[i] = val;
                break;
            }
            case DT_STRING: {
                char buffer[256];
                scanf("%s", buffer);
                values[i] = strdup(buffer);  // strdup handles allocation
                break;
            }
        }
    }

    bool success = InsertRow(table, values);

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (table->Attributes[i].AttributeType == DT_STRING) {
            free(values[i]);  // strdup
        } else {
            free(values[i]);  // INT, FLOAT, UINT
        }
    }
    free(values);

    return success;
}

Table *PromptAndCreateTable() {
    char tableName[100];
    size_t columnCount;

    printf("Enter table name: ");
    scanf("%99s", tableName);  // limit input to avoid buffer overflow

    printf("Enter number of columns: ");
    scanf("%zu", &columnCount);

    Attribute *attributes = malloc(sizeof(Attribute) * columnCount);
    if (!attributes) return NULL;

    for (size_t i = 0; i < columnCount; ++i) {
        char attrName[100];
        int type;

        printf("Enter name for column %zu: ", i + 1);
        scanf("%99s", attrName);

        printf("Select type for '%s':\n", attrName);
        printf(" 0 - INT\n 1 - UINT\n 2 - FLOAT\n 3 - STRING\n");
        printf("Type: ");
        scanf("%d", &type);

        attributes[i].AttributeName = strdup(attrName);
        attributes[i].AttributeType = (DataTypes)type;
    }

    Table *table = CreateTable(tableName, attributes, columnCount);

    for (size_t i = 0; i < columnCount; ++i) {
        free(attributes[i].AttributeName);
    }
    free(attributes);

    return table;
}



bool SaveTableToFile(const Table *table) {
    if (!table) return false;


    MAKE_DIR("data");

    char path[256];
    snprintf(path, sizeof(path), "data/%s.tbl", table->TableName);

    FILE *file = fopen(path, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return false;
    }


    size_t nameLen = strlen(table->TableName);
    fwrite(&nameLen, sizeof(size_t), 1, file);
    fwrite(table->TableName, sizeof(char), nameLen, file);


    fwrite(&table->AttributeCount, sizeof(size_t), 1, file);


    for (size_t i = 0; i < table->AttributeCount; ++i) {
        size_t attrNameLen = strlen(table->Attributes[i].AttributeName);
        fwrite(&attrNameLen, sizeof(size_t), 1, file);
        fwrite(table->Attributes[i].AttributeName, sizeof(char), attrNameLen, file);
        fwrite(&table->Attributes[i].AttributeType, sizeof(DataTypes), 1, file);
    }


    fwrite(&table->RowCount, sizeof(size_t), 1, file);


    for (size_t i = 0; i < table->RowCount; ++i) {
        for (size_t j = 0; j < table->AttributeCount; ++j) {
            void *val = table->Rows[i].values[j];
            switch (table->Attributes[j].AttributeType) {
                case DT_INT:
                    fwrite(val, sizeof(int), 1, file);
                    break;
                case DT_UINT:
                    fwrite(val, sizeof(unsigned int), 1, file);
                    break;
                case DT_FLOAT:
                    fwrite(val, sizeof(float), 1, file);
                    break;
                case DT_STRING: {
                    char *str = (char *)val;
                    size_t len = strlen(str);
                    fwrite(&len, sizeof(size_t), 1, file);
                    fwrite(str, sizeof(char), len, file);
                    break;
                }
            }
        }
    }

    fclose(file);
    printf("Table '%s' saved to disk successfully.\n", table->TableName);
    return true;
}


Table *LoadTableFromFile(const char *filename) {
    if (!filename) return NULL;

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        return NULL;
    }

    Table *table = (Table *)malloc(sizeof(Table));
    if (!table) {
        fclose(file);
        return NULL;
    }
    memset(table, 0, sizeof(Table));


    size_t nameLen = 0;
    fread(&nameLen, sizeof(size_t), 1, file);
    table->TableName = (char *)malloc(nameLen + 1);
    fread(table->TableName, sizeof(char), nameLen, file);
    table->TableName[nameLen] = '\0';


    fread(&table->AttributeCount, sizeof(size_t), 1, file);
    table->Attributes = (Attribute *)malloc(sizeof(Attribute) * table->AttributeCount);


    for (size_t i = 0; i < table->AttributeCount; ++i) {
        size_t attrNameLen = 0;
        fread(&attrNameLen, sizeof(size_t), 1, file);
        table->Attributes[i].AttributeName = (char *)malloc(attrNameLen + 1);
        fread(table->Attributes[i].AttributeName, sizeof(char), attrNameLen, file);
        table->Attributes[i].AttributeName[attrNameLen] = '\0';
        fread(&table->Attributes[i].AttributeType, sizeof(DataTypes), 1, file);
    }


    fread(&table->RowCount, sizeof(size_t), 1, file);
    if (table->RowCount > 0) {
        table->Rows = (Row *)malloc(sizeof(Row) * table->RowCount);
        for (size_t i = 0; i < table->RowCount; ++i) {
            table->Rows[i].values = (void **)malloc(sizeof(void *) * table->AttributeCount);
            for (size_t j = 0; j < table->AttributeCount; ++j) {
                switch (table->Attributes[j].AttributeType) {
                    case DT_INT:
                        table->Rows[i].values[j] = malloc(sizeof(int));
                        fread(table->Rows[i].values[j], sizeof(int), 1, file);
                        break;
                    case DT_UINT:
                        table->Rows[i].values[j] = malloc(sizeof(unsigned int));
                        fread(table->Rows[i].values[j], sizeof(unsigned int), 1, file);
                        break;
                    case DT_FLOAT:
                        table->Rows[i].values[j] = malloc(sizeof(float));
                        fread(table->Rows[i].values[j], sizeof(float), 1, file);
                        break;
                    case DT_STRING: {
                        size_t len = 0;
                        fread(&len, sizeof(size_t), 1, file);
                        table->Rows[i].values[j] = malloc(len + 1);
                        fread(table->Rows[i].values[j], sizeof(char), len, file);
                        ((char *)table->Rows[i].values[j])[len] = '\0';
                        break;
                    }
                }
            }
        }
    } else {
        table->Rows = NULL;
    }

    fclose(file);
    return table;
}

void FilterAndDisplayTable(const Table *table, const char *columnName, const char *valueAsString) {
    if (!table || !columnName || !valueAsString) return;


    size_t columnIndex = -1;
    DataTypes columnType;

    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (strcmp(table->Attributes[i].AttributeName, columnName) == 0) {
            columnIndex = i;
            columnType = table->Attributes[i].AttributeType;
            break;
        }
    }

    if (columnIndex == (size_t)-1) {
        printf("Column '%s' not found in table.\n", columnName);
        return;
    }

    printf("\nFiltered Results (WHERE %s = %s):\n", columnName, valueAsString);

    for (size_t i = 0; i < table->AttributeCount; i++) {
        printf("| %-15s ", table->Attributes[i].AttributeName);
    }
    printf("|\n");

    for (size_t i = 0; i < table->AttributeCount; i++) {
        printf("+-----------------");
    }
    printf("+\n");

    for (size_t i = 0; i < table->RowCount; ++i) {
        Row *row = &table->Rows[i];
        void *val = row->values[columnIndex];
        bool match = false;

        switch (columnType) {
            case DT_INT: {
                int cmpVal = atoi(valueAsString);
                match = (*(int *)val == cmpVal);
                break;
            }
            case DT_UINT: {
                unsigned int cmpVal = (unsigned int)strtoul(valueAsString, NULL, 10);
                match = (*(unsigned int *)val == cmpVal);
                break;
            }
            case DT_FLOAT: {
                float cmpVal = strtof(valueAsString, NULL);
                match = (*(float *)val == cmpVal);
                break;
            }
            case DT_STRING: {
                match = (strcmp((char *)val, valueAsString) == 0);
                break;
            }
        }

        if (match) {
            for (size_t j = 0; j < table->AttributeCount; j++) {
                void *cell = row->values[j];
                switch (table->Attributes[j].AttributeType) {
                    case DT_INT:    printf("| %-15d ", *(int *)cell); break;
                    case DT_UINT:   printf("| %-15u ", *(unsigned int *)cell); break;
                    case DT_FLOAT:  printf("| %-15.2f ", *(float *)cell); break;
                    case DT_STRING: printf("| %-15s ", (char *)cell); break;
                }
            }
            printf("|\n");
        }
    }
}



CompareOperator ParseOperator(const char *op) {
    if (strcmp(op, "=") == 0) return OP_EQ;
    if (strcmp(op, "!=") == 0) return OP_NEQ;
    if (strcmp(op, ">") == 0) return OP_GT;
    if (strcmp(op, "<") == 0) return OP_LT;
    if (strcmp(op, ">=") == 0) return OP_GTE;
    if (strcmp(op, "<=") == 0) return OP_LTE;
    return OP_UNKNOWN;
}



bool Compare(DataTypes type, void *left, const char *rightLiteral, CompareOperator op) {
    switch (type) {
        case DT_INT: {
            int leftVal = *(int *)left;
            int rightVal = atoi(rightLiteral);
            switch (op) {
                case OP_EQ: return leftVal == rightVal;
                case OP_NEQ: return leftVal != rightVal;
                case OP_GT: return leftVal > rightVal;
                case OP_LT: return leftVal < rightVal;
                case OP_GTE: return leftVal >= rightVal;
                case OP_LTE: return leftVal <= rightVal;
                default: return false;
            }
        }
        case DT_UINT: {
            unsigned int leftVal = *(unsigned int *)left;
            unsigned int rightVal = (unsigned int)strtoul(rightLiteral, NULL, 10);
            switch (op) {
                case OP_EQ: return leftVal == rightVal;
                case OP_NEQ: return leftVal != rightVal;
                case OP_GT: return leftVal > rightVal;
                case OP_LT: return leftVal < rightVal;
                case OP_GTE: return leftVal >= rightVal;
                case OP_LTE: return leftVal <= rightVal;
                default: return false;
            }
        }
        case DT_FLOAT: {
            float leftVal = *(float *)left;
            float rightVal = strtof(rightLiteral, NULL);
            switch (op) {
                case OP_EQ: return leftVal == rightVal;
                case OP_NEQ: return leftVal != rightVal;
                case OP_GT: return leftVal > rightVal;
                case OP_LT: return leftVal < rightVal;
                case OP_GTE: return leftVal >= rightVal;
                case OP_LTE: return leftVal <= rightVal;
                default: return false;
            }
        }
        case DT_STRING: {
            int cmp = strcmp((char *)left, rightLiteral);
            switch (op) {
                case OP_EQ: return cmp == 0;
                case OP_NEQ: return cmp != 0;
                case OP_GT: return cmp > 0;
                case OP_LT: return cmp < 0;
                case OP_GTE: return cmp >= 0;
                case OP_LTE: return cmp <= 0;
                default: return false;
            }
        }
        default: return false;
    }
}



bool SelectQuery(Table *table, const char *columnName, const char *operator, const char *valueLiteral) {
    if (!table || !columnName || !operator || !valueLiteral) return false;

    int colIndex = -1;


    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (strcmp(table->Attributes[i].AttributeName, columnName) == 0) {
            colIndex = i;
            break;
        }
    }

    if (colIndex == -1) {
        printf("Column '%s' not found in table '%s'.\n", columnName, table->TableName);
        return false;
    }

    DataTypes type = table->Attributes[colIndex].AttributeType;
    CompareOperator op = ParseOperator(operator);

    if (op == OP_UNKNOWN) {
        printf("Unknown operator: %s\n", operator);
        return false;
    }

    printf("\nMatching rows from table '%s':\n", table->TableName);


    for (size_t i = 0; i < table->AttributeCount; ++i) {
        printf("| %-12s ", table->Attributes[i].AttributeName);
    }
    printf("|\n");


    for (size_t i = 0; i < table->AttributeCount; ++i) {
        printf("+--------------");
    }
    printf("+\n");

    size_t matchCount = 0;
    for (size_t i = 0; i < table->RowCount; ++i) {
        void *cellValue = table->Rows[i].values[colIndex];

        if (Compare(type, cellValue, valueLiteral, op)) {
            matchCount++;
            for (size_t j = 0; j < table->AttributeCount; ++j) {
                switch (table->Attributes[j].AttributeType) {
                    case DT_INT:
                        printf("| %-12d ", *(int *)table->Rows[i].values[j]); break;
                    case DT_UINT:
                        printf("| %-12u ", *(unsigned int *)table->Rows[i].values[j]); break;
                    case DT_FLOAT:
                        printf("| %-12.2f ", *(float *)table->Rows[i].values[j]); break;
                    case DT_STRING:
                        printf("| %-12s ", (char *)table->Rows[i].values[j]); break;
                }
            }
            printf("|\n");
        }
    }

    if (matchCount == 0) {
        printf("No rows matched the condition.\n");
    }

    return true;
}

size_t DeleteRows(Table *table, const char *columnName, const char *operator, const char *valueLiteral) {
    if (!table || !columnName || !operator || !valueLiteral) return 0;

    int colIndex = -1;
    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (strcmp(table->Attributes[i].AttributeName, columnName) == 0) {
            colIndex = i;
            break;
        }
    }
    if (colIndex == -1) {
        printf("Column not found.\n");
        return 0;
    }

    CompareOperator op = ParseOperator(operator);
    if (op == OP_UNKNOWN) {
        printf("Unsupported operator.\n");
        return 0;
    }

    size_t deleted = 0;

    for (size_t i = 0; i < table->RowCount;) {
        void *value = table->Rows[i].values[colIndex];
        if (Compare(table->Attributes[colIndex].AttributeType, value, valueLiteral, op)) {
            // Free each attribute value in row
            for (size_t j = 0; j < table->AttributeCount; ++j) {
                if (table->Attributes[j].AttributeType == DT_STRING) {
                    free(table->Rows[i].values[j]);
                } else {
                    free(table->Rows[i].values[j]);
                }
            }
            free(table->Rows[i].values);


            for (size_t k = i + 1; k < table->RowCount; ++k) {
                table->Rows[k - 1] = table->Rows[k];
            }

            table->RowCount--;
            deleted++;

        } else {
            i++;
        }
    }

    return deleted;
}

size_t UpdateRows(Table *table, const char *targetColumn, const char *newValueLiteral,
                  const char *filterColumn, const char *operator, const char *filterValueLiteral) {
    if (!table || !targetColumn || !newValueLiteral || !filterColumn || !operator || !filterValueLiteral) return 0;

    int filterColIndex = -1, targetColIndex = -1;
    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (strcmp(table->Attributes[i].AttributeName, filterColumn) == 0)
            filterColIndex = i;
        if (strcmp(table->Attributes[i].AttributeName, targetColumn) == 0)
            targetColIndex = i;
    }

    if (filterColIndex == -1 || targetColIndex == -1) {
        printf("Column not found.\n");
        return 0;
    }

    CompareOperator op = ParseOperator(operator);
    if (op == OP_UNKNOWN) {
        printf("Invalid operator.\n");
        return 0;
    }

    size_t updated = 0;

    for (size_t i = 0; i < table->RowCount; ++i) {
        void *filterVal = table->Rows[i].values[filterColIndex];
        if (Compare(table->Attributes[filterColIndex].AttributeType, filterVal, filterValueLiteral, op)) {

            if (table->Attributes[targetColIndex].AttributeType == DT_STRING) {
                free(table->Rows[i].values[targetColIndex]);
                table->Rows[i].values[targetColIndex] = strdup(newValueLiteral);
            } else if (table->Attributes[targetColIndex].AttributeType == DT_INT) {
                int *ptr = malloc(sizeof(int));
                *ptr = atoi(newValueLiteral);
                free(table->Rows[i].values[targetColIndex]);
                table->Rows[i].values[targetColIndex] = ptr;
            } else if (table->Attributes[targetColIndex].AttributeType == DT_UINT) {
                unsigned int *ptr = malloc(sizeof(unsigned int));
                *ptr = (unsigned int)strtoul(newValueLiteral, NULL, 10);
                free(table->Rows[i].values[targetColIndex]);
                table->Rows[i].values[targetColIndex] = ptr;
            } else if (table->Attributes[targetColIndex].AttributeType == DT_FLOAT) {
                float *ptr = malloc(sizeof(float));
                *ptr = strtof(newValueLiteral, NULL);
                free(table->Rows[i].values[targetColIndex]);
                table->Rows[i].values[targetColIndex] = ptr;
            }

            updated++;
        }
    }

    return updated;
}

bool AlterAddColumn(Table *table, const char *columnName, const char *typeStr) {
    if (!table || !columnName || !typeStr) return false;

    DataTypes newType;
    if (strcmp(typeStr, "INT") == 0) newType = DT_INT;
    else if (strcmp(typeStr, "UINT") == 0) newType = DT_UINT;
    else if (strcmp(typeStr, "FLOAT") == 0) newType = DT_FLOAT;
    else if (strcmp(typeStr, "STRING") == 0) newType = DT_STRING;
    else return false;


    Attribute *newAttrs = realloc(table->Attributes, sizeof(Attribute) * (table->AttributeCount + 1));
    if (!newAttrs) return false;

    table->Attributes = newAttrs;
    table->Attributes[table->AttributeCount].AttributeName = strdup(columnName);
    table->Attributes[table->AttributeCount].AttributeType = newType;
    table->AttributeCount++;


    for (size_t i = 0; i < table->RowCount; ++i) {
        void **newValues = realloc(table->Rows[i].values, sizeof(void *) * table->AttributeCount);
        if (!newValues) return false;

        table->Rows[i].values = newValues;


        switch (newType) {
            case DT_INT:
                newValues[table->AttributeCount - 1] = malloc(sizeof(int));
                *((int *)newValues[table->AttributeCount - 1]) = 0;
                break;
            case DT_UINT:
                newValues[table->AttributeCount - 1] = malloc(sizeof(unsigned int));
                *((unsigned int *)newValues[table->AttributeCount - 1]) = 0;
                break;
            case DT_FLOAT:
                newValues[table->AttributeCount - 1] = malloc(sizeof(float));
                *((float *)newValues[table->AttributeCount - 1]) = 0.0f;
                break;
            case DT_STRING:
                newValues[table->AttributeCount - 1] = strdup("");
                break;
        }
    }

    return true;
}

bool AlterDropColumn(Table *table, const char *columnName) {
    if (!table || !columnName) return false;

    int colIndex = -1;
    for (size_t i = 0; i < table->AttributeCount; ++i) {
        if (strcmp(table->Attributes[i].AttributeName, columnName) == 0) {
            colIndex = (int)i;
            break;
        }
    }
    if (colIndex == -1) return false;


    free(table->Attributes[colIndex].AttributeName);


    for (size_t i = colIndex; i < table->AttributeCount - 1; ++i) {
        table->Attributes[i] = table->Attributes[i + 1];
    }

    Attribute *shrunkAttrs = realloc(table->Attributes, sizeof(Attribute) * (table->AttributeCount - 1));
    if (shrunkAttrs) table->Attributes = shrunkAttrs;


    for (size_t i = 0; i < table->RowCount; ++i) {
        free(table->Rows[i].values[colIndex]);
        for (size_t j = colIndex; j < table->AttributeCount - 1; ++j) {
            table->Rows[i].values[j] = table->Rows[i].values[j + 1];
        }

        void **shrunkValues = realloc(table->Rows[i].values, sizeof(void *) * (table->AttributeCount - 1));
        if (shrunkValues) table->Rows[i].values = shrunkValues;
    }

    table->AttributeCount--;
    return true;
}

bool DeleteTableFile(const char *tableName) {
    if (!tableName) return false;

    char filename[200];
    snprintf(filename, sizeof(filename), "data/%s.tbl", tableName);

    if (remove(filename) == 0) {
        printf("File '%s' deleted from disk.\n", filename);
        return true;
    } else {
        printf("Failed to delete '%s'. File may not exist.\n", filename);
        return false;
    }
}






