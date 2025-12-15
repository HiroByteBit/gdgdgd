#ifndef ASSEMBLY_H
#define ASSEMBLY_H

#include <stdio.h>
#include "ast.h"

void AssemblyInit();
void GenerateAssemblyProgram(Node *program, FILE *out);
void GenerateAssemblyNode(Node *node, FILE *out);

#endif