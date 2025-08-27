#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "functions.h"

#define MAX_TABLES 10

int main() {
    Table *tables[MAX_TABLES];
    size_t tableCount = 0;

    char command[100];

    while (1) {
        printf(
            "\nEnter command (CREATE, INSERT, DISPLAY, LIST, SAVE, LOAD, SELECT, DELETE, UPDATE, RENAME, DROP, EXIT): ");
        scanf("%99s", command);

        if (strcmp(command, "CREATE") == 0) {
            if (tableCount >= MAX_TABLES) {
                printf("Max table limit reached.\n");
                continue;
            }
            Table *newTable = PromptAndCreateTable();
            if (newTable) {
                tables[tableCount++] = newTable;
                printf("Table created successfully.\n");
            } else {
                printf("Table creation failed.\n");
            }
        } else if (strcmp(command, "INSERT") == 0) {
            char name[100];
            printf("Enter table name to insert into: ");
            scanf("%99s", name);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, name) == 0) {
                    if (PromptAndInsertRow(tables[i])) {
                        printf("Row inserted successfully.\n");
                    } else {
                        printf("Failed to insert row.\n");
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "DISPLAY") == 0) {
            char name[100];
            printf("Enter table name to display: ");
            scanf("%99s", name);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, name) == 0) {
                    DisplayTable(tables[i]);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "LIST") == 0) {
            ListTablesFromServer();
        } else if (strcmp(command, "SAVE") == 0) {
            char name[100];
            printf("Enter table name to save: ");
            scanf("%99s", name);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, name) == 0) {
                    SaveTableToFile(tables[i]);
                    found = 1;

                    char sendChoice;
                    printf("Do you want to send '%s.tbl' to the server? (y/n): ", name);
                    scanf(" %c", &sendChoice);
                    if (sendChoice == 'y' || sendChoice == 'Y') {
                        SendFileToServer(name);
                    }
                    break;
                }
            }
            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "LOAD") == 0) {
            if (tableCount >= MAX_TABLES) {
                printf("Max table limit reached; cannot load more tables.\n");
                continue;
            }

            char name[100];
            printf("Enter table name to load from server: ");
            scanf("%99s", name);

            Table *loadedTable = LoadTableFromServer(name);
            if (loadedTable) {
                tables[tableCount++] = loadedTable;
                printf("Table '%s' loaded successfully from server and added to memory.\n", loadedTable->TableName);
            } else {
                printf("Failed to load table '%s' from server.\n", name);
            }
        } else if (strcmp(command, "SELECT") == 0) {
            char tableName[100], columnName[100], opStr[3], value[100];

            printf("Enter table name: ");
            scanf("%99s", tableName);

            printf("Do you want to filter results? (yes/no): ");
            char choice[10];
            scanf("%9s", choice);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, tableName) == 0) {
                    found = 1;

                    if (strcmp(choice, "no") == 0) {
                        DisplayTable(tables[i]);
                    } else {
                        printf("Enter column name: ");
                        scanf("%99s", columnName);
                        printf("Enter operator (=, !=, >, <, >=, <=): ");
                        scanf("%2s", opStr);
                        printf("Enter value to compare: ");
                        scanf("%99s", value);
                        SelectQuery(tables[i], columnName, opStr, value);
                    }

                    break;
                }
            }

            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "DELETE") == 0) {
            char tableName[100], columnName[100], opStr[3], value[100];

            printf("Enter table name: ");
            scanf("%99s", tableName);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, tableName) == 0) {
                    found = 1;
                    printf("Enter column name for deletion filter: ");
                    scanf("%99s", columnName);
                    printf("Enter operator (=, !=, >, <, >=, <=): ");
                    scanf("%2s", opStr);
                    printf("Enter value to compare: ");
                    scanf("%99s", value);

                    size_t deleted = DeleteRows(tables[i], columnName, opStr, value);
                    printf("%zu rows deleted.\n", deleted);
                    break;
                }
            }

            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "UPDATE") == 0) {
            char tableName[100], targetColumn[100], newValue[100];
            char filterColumn[100], opStr[3], filterValue[100];

            printf("Enter table name: ");
            scanf("%99s", tableName);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, tableName) == 0) {
                    found = 1;

                    printf("Enter column to update: ");
                    scanf("%99s", targetColumn);
                    printf("Enter new value: ");
                    scanf("%99s", newValue);
                    printf("Enter filter column: ");
                    scanf("%99s", filterColumn);
                    printf("Enter operator (=, !=, >, <, >=, <=): ");
                    scanf("%2s", opStr);
                    printf("Enter filter value: ");
                    scanf("%99s", filterValue);

                    size_t updated = UpdateRows(tables[i], targetColumn, newValue, filterColumn, opStr, filterValue);
                    printf("%zu rows updated.\n", updated);
                    break;
                }
            }

            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "ALTER") == 0) {
            char tableName[100], subCmd[10], columnName[100], typeStr[20];
            printf("Enter table name to alter: ");
            scanf("%99s", tableName);

            int found = -1;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, tableName) == 0) {
                    found = (int) i;
                    break;
                }
            }

            if (found == -1) {
                printf("Table not found.\n");
                continue;
            }

            printf("Enter ALTER command (ADD or DROP): ");
            scanf("%9s", subCmd);

            if (strcmp(subCmd, "ADD") == 0) {
                printf("Enter new column name: ");
                scanf("%99s", columnName);
                printf("Enter type (INT, UINT, FLOAT, STRING): ");
                scanf("%19s", typeStr);
                if (AlterAddColumn(tables[found], columnName, typeStr)) {
                    printf("Column added successfully.\n");
                } else {
                    printf("Failed to add column.\n");
                }
            } else if (strcmp(subCmd, "DROP") == 0) {
                printf("Enter column name to remove: ");
                scanf("%99s", columnName);
                if (AlterDropColumn(tables[found], columnName)) {
                    printf("Column dropped successfully.\n");
                } else {
                    printf("Failed to drop column.\n");
                }
            } else {
                printf("Unknown ALTER operation.\n");
            }
        } else if (strcmp(command, "DROP") == 0) {
            char name[100];
            printf("Enter table name to remove from memory: ");
            scanf("%99s", name);

            int found = -1;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, name) == 0) {
                    FreeTable(tables[i]);
                    for (size_t j = i; j < tableCount - 1; ++j) {
                        tables[j] = tables[j + 1];
                    }
                    tableCount--;
                    found = 1;
                    printf("Table dropped from memory.\n");
                    break;
                }
            }

            if (found == -1) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "RENAME") == 0) {
            char oldName[100], newName[100];
            printf("Enter existing table name: ");
            scanf("%99s", oldName);
            printf("Enter new table name: ");
            scanf("%99s", newName);

            int found = 0;
            for (size_t i = 0; i < tableCount; ++i) {
                if (strcmp(tables[i]->TableName, oldName) == 0) {
                    free(tables[i]->TableName);
                    tables[i]->TableName = strdup(newName);

                    SaveTableToFile(tables[i]);
                    DeleteTableFile(oldName);

                    printf("Table renamed to '%s' and saved.\n", newName);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                printf("Table not found.\n");
            }
        } else if (strcmp(command, "DROPFILE") == 0) {
            char name[100];
            printf("Enter table name to delete from disk: ");
            scanf("%99s", name);
            DeleteTableFile(name);
        } else if (strcmp(command, "EXIT") == 0) {
            printf("Exiting program...\n");
            break;
        } else {
            printf("Unknown command.\n");
        }
    }

    for (size_t i = 0; i < tableCount; ++i) {
        FreeTable(tables[i]);
    }

    return 0;
}
