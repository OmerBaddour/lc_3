; Initialization
              .ORIG     0x3000

              LEA       R0,PROMPT_STRING  ; output PROMPT_STRING
              TRAP      0x0022

              TRAP      0x0023            ; input character to R0
              AND       R4,R0,R0          ; copy character to R4
              
              AND       R0,R0,#0          ; output newline
              ADD       R0,R0,#10
              TRAP      0x0021
              
              LEA       R6,SEARCH_STRING  ; load SEARCH_STRING into R6
              AND       R5,R5,#0          ; character counter in R5


; Test for end of string
; Load character into R1
TEST_END_STR  LDR       R1,R6,#0
              BRz       END


; Test for character equalling search character
; Compute R4 - R1 using ADD(R4, -R1), and -R1 = (NOT R1) + 1
TEST_CHAR     NOT       R1,R1
              ADD       R1,R1,#1
              ADD       R1,R4,R1
              BRnp      NEXT_CHAR
              ADD       R5,R5,#1


; Increment R6 to next character of SEARCH_STRING
NEXT_CHAR     ADD       R6,R6,#1
              BRnzp     TEST_END_STR


; End: print character counter value and halt
END           LEA       R0,COUNT_STRING   ; output COUNT_STRING
              TRAP      0x0022

              AND       R0,R5,R5          ; copy character counter in R5 to R0
              LD        R1,ASCII
              ADD       R0,R0,R1
              TRAP      0x0021            ; output character counter
              
              AND       R0,R0,#0          ; output newline
              ADD       R0,R0,#10
              TRAP      0x0021
              
              TRAP      0x0025            ; halt


; Data
PROMPT_STRING .STRINGZ  "Enter character to search: "
SEARCH_STRING .STRINGZ  "Hello world"
COUNT_STRING  .STRINGZ  "Character count: "
ASCII         .FILL     0x0030            ; adding 0x30 to a value gives its ASCII code


              .END                        ; end assembly file
