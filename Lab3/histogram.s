    PUBLIC EightBitHistogram

    SECTION .text : CODE (2)
    THUMB
; os registradores de cada parametro passado sao na seguinte ordem:
; R0, R1, R2 e R3

; funcao que monta o histograma e verifica se continua calculo
EightBitHistogram:
    PUSH    {R8, R9, R10, R11}    ; salvando pra pilha os registradores que usarei
    MUL     R8, r0, r1            ; multiplicando height e a width pra saber o tamanho da imagem
    MOV     R9, #65535            ; armazenando valor do tamanho maximo aceito
    CMP     R8, R9                ; compara tamanho da imagem e tamanho maximo
    BGT     TooBigImage           ; branch pra funcao de erro caso seja greater then
    MOV     R9, #0                ; seta o R9 como o iterador do histograma

ZeroHist:
    MOV     R10, #0               
    STRH    R10, [R3, R9, LSL#1]  ; zera o elemento no indice R9
    ADD     R9, R9, #1            ; incrementa o iterador
    CMP     R9, #256              ; compara iterador com o tamanho do vetor
    BNE     ZeroHist              ; nao sendo igual, faz o loop na funcao
    MOV     R9, #0                ; sendo igual, apenas zera o R9 para continuar como iterator

FillHist:
    LDRB    R10, [R2, R9]            ; R10 recebe valor do pixel da imagem
    LDRH    R11, [R3, R10, LSL#1]    ; carrega valor do hist naquela posicao pro R11
    ADD     R11, R11, #1             ; incrementa esse valor em 1
    STRH    R11, [R3, R10, LSL#1]    ; insere o valor no hist
    ADD     R9, R9, #1               ; incrementa iterador R9
    CMP     R9, R8                   ; compara iterador com o tamanho da imagem
    BNE     FillHist                 ; caso not equal, faz o loop
    MOV     R0, R8                   ; contratio, retorna tamanho da imagem
    POP     {R8, R9, R10, R11}       ; volta registradores utilizados da pilha
    BX      LR

TooBigImage:
    POP     {R8, R9, R10, R11}    ; volta registradores utilizados da pilha
    MOV     R0, #0                ; retorna 0 na funcao de erro
    BX      LR

    END