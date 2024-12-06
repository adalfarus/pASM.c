#include "pconstants.h"

const char *INSTRUCTION_SET[] = {
    [00]="NOP",     [10]="LDA_IMM", [11]="LDA_DIR", [12]="LDA_IND", 
    [20]="STA_DIR", [21]="STA_IND", [30]="ADD_DIR", [40]="SUB_DIR", 
    [50]="MUL_DIR", [60]="DIV_DIR", [70]="JMP_DIR", [71]="JMP_IND", 
    [80]="JNZ_DIR", [81]="JNZ_IND", [90]="JZE_DIR", [91]="JZE_IND", 
    [92]="JLE_DIR", [93]="JLE_IND", [99]="STP"
};
