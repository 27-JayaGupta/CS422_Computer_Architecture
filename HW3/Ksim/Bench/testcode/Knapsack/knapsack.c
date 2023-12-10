#include <stdio.h>

int main() {
    int a4_value;
    asm inline(
        "addiu $sp, $sp, -40\n\t"   
        "lui $4, 0x1234\n\t"         
	    "ori $4, $4, 0x5678\n\t"
        "swl $4, 11($sp)\n\t"         
	    "swr $4, 15($sp)\n\t"
	    "lwr $5, 15($sp)\n\t"
        "lwl $5, 11($sp)\n\t"
        "addiu $5, $5, 0x5678\n\t"    
        "addiu $sp, $sp, 40\n\t"        
    );

    // Print the value in $5
    asm ("move %0, $5" : "=r" (a4_value));
    printf("Value in $5: %08x\n", a4_value);

    return 0;
}

