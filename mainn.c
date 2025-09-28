#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <struct.h>
int main()
{
    int choice, s;
    while (1)
    {
        printf("\n 1.Enter ten numbers in arrays");
        printf("\n 2.Enter to calculate the sum and avg of total numbers in th arrays of ten numbers");
        printf("\n 3.Even odd arrays");
        printf("\n 4.Exit");
        // printf("\n 0.exit");
        printf("\nEnter your choice :  ");
        scanf("%d", &choice);
        switch (choice)
        {
        case 1:
            f1();
            break;

        case 2:
            f2();
            break;

        case 3:
            f3();
            break;

        case 4:
            exit(0);
        }
    }
    return 0;
}
