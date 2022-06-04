#include "proj.h"

//Method to read standard input from the clients 
int read_stdin(char *buf, int buf_len, int *more) {

    if(!fgets(buf, buf_len, stdin)) { 

        *more = 0;
    }

    int len = strlen(buf);
    if(buf[len - 1] == '\n'){
        
        *more = 0;
    }
    
    return len;
}

//Method to print the bits of the headers (used for testing)
void print_u16_as_bits(uint16_t x) {

    printf("%u: ", x);
    for (int i = 15; i >= 0; i--) {

        printf("%u ", (x & (0x0001 << i)) >> i);
    }

    printf("\n");
}

//Parses the MT bit of the header (used to check if control or chat header)
int parse_mt_bit(uint8_t *header_byte1) {
    
    if(!header_byte1){

        return -1;
    }

    return (*header_byte1 & (0x1 << 7)) != 0 ? 1 : 0;
}


//This method simply parses the control header. (Or, unpacks all the data and assigns the correct value for variables)
int parse_control_header(uint16_t *header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *ulen) {

    if(header == NULL){

        return -1;
    }
    
    *mt = (*header & (0x1 << 15)) != 0 ? 1 : 0;
    *code = ((*header & (0xF << 11)) >> 11);
    *unc = (*header & (0xF << 4)) >> 4;
    *ulen = (*header & 0xF);

    return 1; 
}

//This method simply packs all the fields for the control header. We simply shift all bits to the left
int create_control_header(uint16_t *_header, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen) {

    if(!_header){

        return -1;
    }
        
    *_header = 0;
    *_header = (*_header | (mt << 15));
    *_header = (*_header | (code << 11));
    *_header = (*_header | (unc << 4));
    *_header = (*_header | (ulen));

    return 1;
}

//Unpacks all the fields in the chat header and assigns their values 
int parse_chat_header(uint32_t *header, uint8_t *mt, uint8_t *pub, uint8_t *prv, uint8_t *frg, uint8_t *lst, uint8_t *ulen, uint16_t *length) {

    if(!header || !mt || !pub || !prv || !frg || !lst || !ulen || !length){

        return 0;
    } 

    *mt = (*header & (0x1 << 31)) != 0 ? 1 : 0;
    *pub = (*header & (0x1 << 30)) >> 30;
    *prv = (*header & (0x1 << 29)) >> 29;
    *frg = (*header & (0x1 << 28)) >> 28;
    *lst = (*header & (0x1 << 27)) >> 27;
    *ulen = (*header & (0xF << 12)) >> 12;
    *length = *header & (0xFFF); 

    return 1;
}

//Creates chat header. Shift all bits to the left and add them to the header 
int create_chat_header(uint32_t *header, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length) {

    if(!header){

        return -1;
    }

    *header = 0;
    *header = (*header | (mt << 31));
    *header = (*header | (pub << 30));
    *header = (*header | (prv << 29));
    *header = (*header | (frg << 28));
    *header = (*header | (lst << 27));
    *header = (*header | (ulen << 12));
    *header = (*header | (length));
    
    return 1;
}
