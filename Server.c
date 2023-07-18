#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include "headers.h"


void unlock(int fd, struct flock lock){ // Used to unlock any lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
}

void readLockProduct(int fd, struct flock lock){ // Read lock on inventory 

    lock.l_len = 0;
    lock.l_type = F_RDLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    fcntl(fd, F_SETLKW, &lock);

}

void writeLockProduct (int fd, struct flock lock, int ch) { // record lock
    if (ch == 1) {
        // records the product whose end we at
        // otherwise the product whose starting we at
        lseek(fd, (-1) * sizeof(struct product), SEEK_CUR);
    }
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_CUR;
    lock.l_start = 0;
    lock.l_len = sizeof(struct product);
    fcntl(fd, F_SETLKW, &lock);
}

void cartRecordLock(int fd_cart, struct flock lock_cart, int offset, int ch){
    lock_cart.l_whence = SEEK_SET;
    lock_cart.l_len = sizeof(struct cart);
    lock_cart.l_start = offset;
    if (ch == 1){
        //rdlck
        lock_cart.l_type = F_RDLCK;
    }else{
        //wrlck
        lock_cart.l_type = F_WRLCK;
    }
    fcntl(fd_cart, F_SETLKW, &lock_cart);
    lseek(fd_cart, offset, SEEK_SET);
}

int getCartOffset(int cust_id, int fd_customer){
    if (cust_id < 0){
        return -1;
    }
    struct flock lock_cust; 

    // setLockCust(fd_customer, lock_cust);
    // Read locking customers.txt to get offset for cart
    lock_cust.l_len = 0;
    lock_cust.l_type = F_RDLCK;
    lock_cust.l_start = 0;
    lock_cust.l_whence = SEEK_SET;
    fcntl(fd_customer, F_SETLKW, &lock_cust);

    struct index id;

    while (read(fd_customer, &id, sizeof(struct index))){
        if (id.custid == cust_id){
            unlock(fd_customer, lock_cust);
            return id.offset;
        }
    }
    unlock(fd_customer, lock_cust);
    return -1;
}

void generateAdminReceipt(int fd_adminReceipt, int fd){
    struct flock lock;
    readLockProduct(fd, lock);
    write(fd_adminReceipt, "Current Inventory:\n", strlen("Current Inventory:\n"));
    write(fd_adminReceipt, "ProductID\tProductName\tQuantity\tPrice\n", strlen("ProductID\tProductName\tQuantity\tPrice\n"));

    lseek(fd, 0, SEEK_SET);
    struct product p;
    while (read(fd, &p, sizeof(struct product))){
        if (p.id != -1){
            char temp[100];
            sprintf(temp, "%d\t%s\t%d\t%d\n",p.id, p.name, p.qty, p.price);
            write(fd_adminReceipt, temp, strlen(temp));
        }
    }
    unlock(fd, lock);
}

void updateProduct(int fd_in, int new_fd, int ch, int fd_adminReceipt) {
    struct product p1;
    read(new_fd, &p1, sizeof(struct product));
    int id = p1.id, val = -1;
    char response[100];

    if (ch == 1) {
        val = p1.price;
    } else {
        val = p1.qty;
    }

    struct flock lock;
    readLockProduct(fd_in, lock); // to search for the product to be updated
    int flag = 0;
    struct product p;
    while (read(fd_in, &p, sizeof(struct product))) {
        if (p.id == id) {
            unlock(fd_in, lock); // found and hence no need to lock the whole file
            writeLockProduct(fd_in, lock, 1);
            int old;
            if (ch == 1) {
                old = p.price;
                p.price = val;
            } else {
                old = p.qty;
                p.qty = val;
                if (val == 0) {
                    p.id = -1;
                    p.price = -1;
                }
            }

            write(fd_in, &p, sizeof(struct product));
            unlock(fd_in, lock);
            if (ch == 1) {
                write(new_fd, "Price updated successfully\n", sizeof("Price updated successfully\n"));
                sprintf(response, "Price of product with product id %d updated from %d to %d\n", id, old, val);
                write(fd_adminReceipt, response, strlen(response));
            } else {
                write(new_fd, "Quantity updated successfully\n", sizeof("Quantity updated successfully\n"));
                sprintf(response, "Quantity of product with product id %d updated from %d to %d\n", id, old, val);
                write(fd_adminReceipt, response, strlen(response));
            }

            flag = 1;
            break;
        }
    }

    if (!flag) {
        unlock(fd_in, lock);
        write(new_fd, "Product id invalid\n", sizeof("Product id invalid\n"));
        sprintf(response, "Update of product with product id %d unsuccessful\n", id);
        write(fd_adminReceipt, response, strlen(response));
    }
}

int min (int a, int b) {
    if (b < a) {
        a = b;
    }
    return a;
}


int main(){

    int fd = open("inventory.txt", O_RDWR | O_CREAT, 0777); 
    int fd_cart = open("carts.txt", O_RDWR | O_CREAT, 0777);
    int fd_customer = open("customers.txt", O_RDWR | O_CREAT, 0777);
    int fd_adminReceipt = open("adminReceipt.txt", O_RDWR | O_CREAT, 0777);
    lseek(fd_adminReceipt, 0, SEEK_END);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1){
        perror("Error: ");
        return -1;
    }

    struct sockaddr_in serv, cli;
    
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(5555);

    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("Error: ");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&serv, sizeof(serv)) == -1){
        perror("Error: ");
        return -1;
    }

    if (listen(sockfd, 5) == -1){
        perror("Error: ");
        return -1;
    }

    int size = sizeof(cli);
    printf("Server set up successfully\n");

    while (1){

        int new_fd = accept(sockfd, (struct sockaddr *)&cli, &size);
        if (new_fd == -1){
            return -1;
        }

        if (!fork()){
            printf("Connection established successful with the client\n");
            close(sockfd);

            int user;
            read(new_fd, &user, sizeof(int));
            
            if (user == 1){

                int i;

                while (1){
                    read(new_fd, &i, sizeof(int));

                    lseek(fd, 0, SEEK_SET);
                    lseek(fd_cart, 0, SEEK_SET);
                    lseek(fd_customer, 0, SEEK_SET);

                    if (i == 1){
                        close(new_fd);
                        break;
                    }
                    else if (i == 2){

                        struct flock lock;
                        readLockProduct(fd, lock);

                        struct product p;
                        while (read(fd, &p, sizeof(struct product))){
                            if (p.id != -1){
                                write(new_fd, &p, sizeof(struct product));
                            }
                        }
                        
                        p.id = -1; // For End of File
                        write(new_fd, &p, sizeof(struct product));
                        unlock(fd, lock);
                    }

                    else if (i == 3){

                        // viewCart
                        int cust_id = -1;
                        read(new_fd, &cust_id, sizeof(int));

                        int offset = getCartOffset(cust_id, fd_customer);   
                        write(new_fd, &offset, sizeof(int)); 

                        if (offset == -1){
                            continue; 
                        }

                        else{
                            struct cart c;
                            struct flock lock_cart;
                            
                            cartRecordLock(fd_cart, lock_cart, offset, 1);
                            read(fd_cart, &c, sizeof(struct cart));
                            unlock(fd_cart, lock_cart);

                            write(new_fd, &c, sizeof(struct cart));
                            
                        }
                    }

                    else if (i == 4){

                        int cust_id = -1;
                        read(new_fd, &cust_id, sizeof(int));
                        int offset = getCartOffset(cust_id, fd_customer);

                        write(new_fd, &offset, sizeof(int));

                        if (offset == -1){
                            continue;
                        }

                        struct product p;
                        read(new_fd, &p, sizeof(struct product));

                        struct flock lock_cart; // Reading Cart from carts.txt
                        cartRecordLock(fd_cart, lock_cart, offset, 1);
                        struct cart c;
                        read(fd_cart, &c, sizeof(struct cart));
                        unlock(fd_cart, lock_cart);

                        int flg1 = 0;
                        for (int i=0; i<MAX_PROD; i++){ // To check whether product already in cart or not
                            if (c.products[i].id == p.id){
                                flg1 = 1;
                                break;
                            }
                        }

                        if (flg1){
                            char response[] = "Product is already in cart!\n";
                            write(new_fd, response, sizeof(response));
                            continue;
                        }

                        struct flock lock_prod;
                        readLockProduct(fd, lock_prod);

                        struct product p1;
                        int found = 0;
                        while (read(fd, &p1, sizeof(struct product))){
                            if (p1.id == p.id && p1.qty >= p.qty) {
                                    found = 1;
                                    break;
                            }
                        }

                        unlock(fd, lock_prod);

                        if (!found){
                            char response[] = "Invalid Product or Invalid Quantity!\n";
                            write(new_fd, response, sizeof(response));
                            continue;
                        }
                        
                        
                        int flg = 0;
                        for (int i=0; i<MAX_PROD; i++){ 
                            if (c.products[i].id == -1 || c.products[i].qty<=0){ // -1 indicates free space available in cart
                                flg = 1;
                                c.products[i].id = p.id;
                                c.products[i].qty = p.qty;
                                strcpy(c.products[i].name, p1.name);
                                c.products[i].price = p1.price;
                                break;

                            }
                        }

                        if (!flg){
                            char response[] = "Cart Limit Exceeded!\n";
                            write(new_fd, response, sizeof(response));
                            continue;
                        }

                        
                        cartRecordLock(fd_cart, lock_cart, offset, 2);    
                        write(fd_cart, &c, sizeof(struct cart));
                        unlock(fd_cart, lock_cart);

                        write(new_fd, "Success!\n", sizeof("Success!\n"));

                    }

                    else if (i == 5){
                        
                        int cust_id = -1;
                        read(new_fd, &cust_id, sizeof(int));

                        int offset = getCartOffset(cust_id, fd_customer);
                        write(new_fd, &offset, sizeof(int));
                        if (offset == -1) {
                            continue;
                        }

                        struct product p;
                        read(new_fd, &p, sizeof(struct product));

                        struct flock lock_cart;
                        cartRecordLock(fd_cart, lock_cart, offset, 1);
                        struct cart c;
                        read(fd_cart, &c, sizeof(struct cart));

                        // int flag = 0, i;
                        // for (i = 0; i < MAX_PROD; i++) {
                        //     if (c.products[i].id == p.id) { // given productid is in cart
                        //         struct flock lock_prod;
                        //         readLockProduct(fd, lock_prod);
                        //         struct product p1;
                        //         while (read(fd, &p1, sizeof(struct product))) {
                        //             if (p1.id == p.id && p1.qty >= p.qty) {
                        //                 flag = 1;
                        //                 break;
                        //             }
                        //         }
                        //         unlock(fd, lock_prod);
                        //         break;
                        //     }
                        // }
                        // unlock(fd_cart, lock_cart);

                        // if (!flag) {
                        //     char response[] = "Update Failed.\n";
                        //     write(new_fd, response, sizeof(response));
                        //     continue;
                        // } 

                        // c.products[i].qty = p.qty;
                        // cartRecordLock(fd_cart, lock_cart, offset, 2); // write lock
                        // write(fd_cart, &c, sizeof(struct cart));
                        // unlock(fd_cart, lock_cart);

                        // char response[] = "Update Successful.\n";
                        // write(new_fd, response, sizeof(response));
                        
                        int flag = 0, i;
                        for (i = 0; i < MAX_PROD; i++) {
                            if (c.products[i].id == p.id) { // given productid is in cart
                                flag = 1;
                                break;
                            }
                        }
                        unlock(fd_cart, lock_cart);

                        if (!flag) {
                            char response[] = "Product not in cart.\n";
                            write(new_fd, response, sizeof(response));
                            continue;
                        } 

                        flag = 0;
                        struct flock lock_prod;
                        readLockProduct(fd, lock_prod);
                        struct product p1;
                        while (read(fd, &p1, sizeof(struct product))) { // to check if the quantity is valid
                            if (p1.id == p.id && p1.qty >= p.qty) {
                                c.products[i].qty = p.qty;
                                flag = 1;
                                break;
                            }
                        }
                        unlock(fd, lock_prod);

                        if (!flag) {
                            char response[] = "Quantity too large.\n";
                            write(new_fd, response, sizeof(response));
                            continue;
                        } 
                        cartRecordLock(fd_cart, lock_cart, offset, 2); // write lock
                        write(fd_cart, &c, sizeof(struct cart));
                        unlock(fd_cart, lock_cart);

                        char response[] = "Update Successful.\n";
                        write(new_fd, response, sizeof(response));
                        
                    }

                    else if (i == 6){
                        int cust_id = -1;
                        read(new_fd, &cust_id, sizeof(int));

                        int offset;
                        offset = getCartOffset(cust_id, fd_customer);
                        write(new_fd, &offset, sizeof(int));
                        if (offset == -1) {
                            continue;
                        }

                        struct flock lock_cart;
                        cartRecordLock(fd_cart, lock_cart, offset, 1); // read lock
                        struct cart c;
                        read(fd_cart, &c, sizeof(struct cart));
                        unlock(fd_cart, lock_cart);
                        write(new_fd, &c, sizeof(struct cart));

                        struct flock lock_prod;
                        readLockProduct(fd, lock_prod);
                        int sum = 0;
                        for (int i = 0; i < MAX_PROD; i++) { // tells how many of each product 
                            if (c.products[i].id != -1) { // product is in cart
                                write(new_fd, &c.products[i].qty, sizeof(int));
                                lseek(fd, 0, SEEK_SET);

                                struct product p;
                                int flag = 0;
                                while (read(fd, &p, sizeof(struct product))) { // searching for the product in inventory file
                                    if (p.id == c.products[i].id && p.qty > 0) {
                                        int mn = min(p.qty, c.products[i].qty); 
                                        // someone else buys some of the items 
                                        flag = 1;
                                        write(new_fd, &mn, sizeof(int));
                                        write(new_fd, &p.price, sizeof(int));
                                        break;
                                    }
                                }

                                if (!flag) {  // gets deleted 
                                    int val = 0;
                                    write(new_fd, &val, sizeof(int));
                                    write(new_fd, &val, sizeof(int));
                                }
                            }
                        }
                        unlock(fd, lock_prod);

                        int proceed;
                        read(new_fd, &proceed, sizeof(int));

                        for (int i = 0; i < MAX_PROD; i++) { // changing values 
                            struct flock lock_prod;
                            readLockProduct(fd, lock_prod);
                            lseek(fd, 0, SEEK_SET);
                            int flag = 0;

                            struct product p;
                            while (read(fd, &p, sizeof(struct product))) {
                                if (p.id == c.products[i].id) {
                                    int mn = min(p.qty, c.products[i].qty); 
                                    unlock(fd, lock_prod);
                                    writeLockProduct(fd, lock_prod, 1); // only locks the item we need to change
                                    p.qty = p.qty - mn;
                                    if (p.qty == 0) {
                                        p.id = -1;
                                    }
                                    write(fd, &p, sizeof(struct product));
                                    unlock(fd, lock_prod);
                                    flag = 1;
                                    break;
                                }
                            }
                            if (!flag) {
                                unlock(fd, lock_prod);
                            }
                        }

                        cartRecordLock(fd_cart, lock_cart, offset, 2); // write lock
                        for (int i = 0; i < MAX_PROD; i++) { // emptying cart
                            c.products[i].id = -1;
                            strcpy(c.products[i].name, "");
                            c.products[i].price = -1;
                            c.products[i].qty = -1;
                        }

                        write(fd_cart, &c, sizeof(struct cart)); // writing empty cart to file
                        unlock(fd_cart, lock_cart);

                        read(new_fd, &sum, sizeof(int));
                        read(new_fd, &c, sizeof(struct cart));

                        int fd_rec = open("receipt.txt", O_CREAT | O_RDWR, 0777);
                        char temp[1000];
                        for (int i = 0; i < MAX_PROD; i++) {
                            if (c.products[i].id != -1) {
                                sprintf(temp, "ProductID: %d\nProduct Name: %s\nQuantity: %d\nPrice: %d\n", c.products[i].id, c.products[i].name, c.products[i].qty, c.products[i].price);
                                write(fd_rec, temp, strlen(temp));
                            }
                        }
                        sprintf(temp, "Total Sum: %d\n\n", sum);
                        write(fd_rec, temp, strlen(temp));
                        close(fd_rec);
                    }

                    else if (i == 7){

                        struct flock lock;
                        // Read locking customers.txt
                        lock.l_len = 0;
                        lock.l_type = F_RDLCK;
                        lock.l_start = 0;
                        lock.l_whence = SEEK_SET;
                        fcntl(fd_customer, F_SETLKW, &lock);

                        // setLockCust(fd_customer, lock);
                        
                        int max_id = -1; 
                        struct index id ;
                        while (read(fd_customer, &id, sizeof(struct index))){
                            if (id.custid > max_id){
                                max_id = id.custid;
                            }
                        }

                        max_id ++;
                        
                        id.custid = max_id;
                        id.offset = lseek(fd_cart, 0, SEEK_END);
                        lseek(fd_customer, 0, SEEK_END);
                        write(fd_customer, &id, sizeof(struct index)); // Initializing for customer ndx file

                        struct cart c;
                        c.custid = max_id;
                        for (int i=0; i<MAX_PROD; i++){ // Initalizing cart structure
                            c.products[i].id = -1;
                            strcpy(c.products[i].name , "");
                            c.products[i].qty = -1;
                            c.products[i].price = -1;
                        }

                        lseek(fd_cart,0,SEEK_END);
                        write(fd_cart, &c, sizeof(struct cart));
                        unlock(fd_customer, lock);
                        write(new_fd, &max_id, sizeof(int));

                    }
                }

                printf("Connection terminated\n");

            }

            else if (user == 2){
                
                int i;
                while (1){
                    read(new_fd, &i, sizeof(int));

                    lseek(fd, 0, SEEK_SET);
                    lseek(fd_cart, 0, SEEK_SET);
                    lseek(fd_customer, 0, SEEK_SET);

                    if (i == 1){ // Changed locking

                        char response[100];

                        struct product p1;
                        read(new_fd, &p1, sizeof(struct product));
                        struct flock lock;
                        readLockProduct(fd, lock);

                        struct product p;

                        int flg = 0;
                        while (read(fd, &p, sizeof(struct product))){

                            if (p.id == p1.id && p.qty > 0){
                                write(new_fd, "Failed\n", sizeof("Failed\n"));
                                sprintf(response, "Adding product with product id %d failed as product already exists\n", p1.id);
                                write(fd_adminReceipt, response, strlen(response));
                                unlock(fd, lock);
                                flg = 1;
                                break;
                            }
                        }

                        if (flg) {
                            continue;
                        }

                        lseek(fd, 0, SEEK_END);
                        
                        writeLockProduct(fd, lock, 0);
                        write(fd, &p1, sizeof(struct product));
                        unlock(fd, lock); 

                        char response2[] = "Success!\n";
                        write(new_fd, response2, sizeof(response2));
                        sprintf(response, "New product with product id %d added successfully\n", p1.id);
                        write(fd_adminReceipt, response, strlen(response));  

                    } 

                    else if (i == 2){

                        int id;
                        read(new_fd, &id, sizeof(int));

                        struct flock lock;
                        readLockProduct(fd, lock);
                        char response[100];

                        struct product p;
                        int flg = 0;

                        while (read(fd, &p, sizeof(struct product))){
                            if (p.id == id){
                                
                                unlock(fd, lock);
                                writeLockProduct(fd, lock, 1); // just locking the particular product

                                p.id = -1; // Deleting means make everything -1
                                strcpy(p.name, "");
                                p.price = -1;
                                p.qty = -1;

                                write(fd, &p, sizeof(struct product));
                                write(new_fd, "Success!", sizeof("Success!"));
                                sprintf(response, "Product with product id %d deleted succesfully!\n", id);
                                write(fd_adminReceipt, response, strlen(response));

                                unlock(fd, lock);
                                flg = 1;
                                break;
                            }
                        }

                        if (!flg){
                            unlock(fd, lock);
                            sprintf(response, "Deleting product with product id %d failed!\n", id);
                            write(fd_adminReceipt, response, strlen(response));
                            write(new_fd, "Failed!", sizeof("Failed!")); // Product ID is invalid
                        }
                    }

                    else if (i == 3){
                        updateProduct(fd, new_fd, 1, fd_adminReceipt);
                    }

                    else if (i == 4){
                        updateProduct(fd, new_fd, 2, fd_adminReceipt);
                    }

                    else if (i == 5){

                        struct flock lock;
                        readLockProduct(fd, lock);

                        struct product p;
                        while (read(fd, &p, sizeof(struct product))){
                            if (p.id != -1){
                                write(new_fd, &p, sizeof(struct product));
                            }
                        }
                        
                        p.id = -1; // For end of file
                        write(new_fd, &p, sizeof(struct product));
                        unlock(fd, lock);
                    }

                    else if (i == 6){
                        close(new_fd);
                        generateAdminReceipt(fd_adminReceipt, fd);
                        break;
                    }
                    else{
                        continue;
                    }
                }
            }
            printf("Connection terminated\n");

        }else{
            close(new_fd);
        }
    }
}