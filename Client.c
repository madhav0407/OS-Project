#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "headers.h"
#include <fcntl.h>
#include <sys/stat.h>

int productInput(){

    int productId;
    while (1){

        printf("Please enter a valid product ID, it should be a non-negative integer: ");
        scanf("%d", &productId);

        if (productId < 0){
            printf("Invalid product ID. Please try again.\n");
        } else {
            break;
        }
    }
    return productId;
}

int priceInput(){

    int price;
    while (1){
        printf("Please enter the price of the product, it should be a non-negative integer: ");
        scanf("%d", &price);

        if (price < 0){
            printf("Invalid price. The price cannot be negative. Please try again.\n");
        } else {
            break;
        }
    }
    return price;
}

int qtyInput(){
    
    int qty;
    while (1){
        printf("Enter quantity: ");
        scanf("%d", &qty);

        if (qty < 0){
            printf("Quantity is not allowed to be negative, try again\n");
        }else{
            break;
        }

    }
    return qty;
}

int main(){

    // This line creates a new socket for the client to connect to the server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // This if statement checks if the socket was created successfully
    if (sockfd == -1){
        // If there was an error creating the socket, it prints an error message
        perror("Error: ");
        // And returns -1 to indicate that there was an error
        return -1;
    }

    // This block of code sets the IP address and port number of the server
    // The IP address is set to INADDR_ANY, which means it will connect to any available network interface
    // The port number is set to 5555
    struct sockaddr_in serv;
    
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(5555);

    // This line attempts to connect to the server using the socket and server information provided
    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) == -1){
        perror("Error: ");
        return -1;
    }

    printf("Please select which menu you would like to access by entering 1 for User Menu or 2 for Admin Menu.\n");
    int user;
    scanf("%d", &user);
    write(sockfd, &user, sizeof(user));

    if (user == 1){

        while (1){

            // Header
            printf("============================================\n");
            printf("|              WELCOME TO OUR STORE         |\n");
            printf("============================================\n");

            // Menu options
            printf("| 1. Exit                                    |\n");
            printf("| 2. View all available products             |\n");
            printf("| 3. View your cart                          |\n");
            printf("| 4. Add product to your cart                |\n");
            printf("| 5. Edit an existing product in cart        |\n");
            printf("| 6. Proceed to checkout                     |\n");
            printf("| 7. Register as a new customer              |\n");

            printf("|-------------------------------------------|\n");
            printf("| Please enter a number to select an option: |\n");
            printf("============================================\n");

            // char ch;
            // scanf("%c",&ch);
            // scanf("%c",&ch);

            int i;
            scanf("%i",&i);

            write(sockfd, &i, sizeof(int));

            if (i == 1){
                printf("Exiting the menu\n");
                break;
            }

            else if (i == 2){
                // Print table header
                printf("====================================================================\n");
                printf("| Product ID | Product Name        | Quantity in Stock | Price |\n");
                printf("====================================================================\n");

                // Read and print each product until the end of file is reached
                while (1){
                    struct product p;
                    read(sockfd, &p, sizeof(struct product));

                    if (p.id != -1 && p.qty > 0){
                        printf("| %10d | %20s | %17d | %9d |\n", p.id, p.name, p.qty, p.price);
                    }
                    else {
                        // End of file reached
                        break;
                    }
                }

                // Print table footer
                printf("====================================================================\n");

               
            }

            else if (i == 3){
                // Get customer ID from user input
                int cusid;
                printf("Enter your customerID: ");
                scanf("%d",&cusid);

                // Send customer ID to server
                write(sockfd, &cusid, sizeof(int));

                 // Check if customer ID is valid
                int is_valid_customer;
                read(sockfd, &is_valid_customer, sizeof(int));
                if (is_valid_customer == -1) {
                    printf("Invalid customer ID. Please try again.\n");
                    continue;
                }

                // Read cart information from server
                struct cart o;
                read(sockfd, &o, sizeof(struct cart));

                if (o.custid != -1){
                    // Print customer ID and table header
                    printf("Customer ID: %d\n", o.custid);
                    printf("=======================================================\n");
                    printf("| Product ID | Product Name        | Quantity | Price |\n");
                    printf("=======================================================\n");
                    
                    // Print product information in table format for each product in the cart
                    for (int i=0; i<MAX_PROD; i++){
                        if (o.products[i].id != -1 && o.products[i].qty > 0){
                            printf("| %10d | %20s | %8d | %9d |\n", o.products[i].id, o.products[i].name, o.products[i].qty, o.products[i].price);
                        }
                    }

                    // Print table footer
                    printf("=======================================================\n");

                } else {
                    // Cart not found for provided customer ID
                    printf("Cart not found for customer ID %d\n", cusid);
                }

            }

            else if (i == 4){
                
                int customer_id;
                printf("Enter your customerID: ");
                scanf("%d", &customer_id);

                write(sockfd, &customer_id, sizeof(int));

                // Check if customer ID is valid
                int is_valid_customer;
                read(sockfd, &is_valid_customer, sizeof(int));
                if (is_valid_customer == -1) {
                    printf("Invalid customer ID. Please try again.\n");
                    continue;
                }

                // Get product ID and quantity from user input
                int product_id = productInput();
                int quantity = qtyInput();

                // Create product object
                struct product p;
                p.id = product_id;
                p.qty = quantity;

                // Send product to server
                write(sockfd, &p, sizeof(struct product));

                // Receive response from server and print it
                char response[100];
                read(sockfd, response, sizeof(response));
                printf("%s", response);
            }

            else if (i == 5){

                int customer_id;
                printf("Enter your customerID: ");
                scanf("%d",&customer_id);
                write(sockfd, &customer_id, sizeof(int));

                // Check if customer ID is valid
                int is_valid_customer;
                read(sockfd, &is_valid_customer, sizeof(int));
                if (is_valid_customer == -1) {
                    printf("Invalid customer ID. Please try again.\n");
                    continue;
                }

                // Get product ID and quantity from user input
                int product_id = productInput();
                int quantity = qtyInput();

                // Create product object
                struct product p;
                p.id = product_id;
                p.qty = quantity;

                // Send product to server
                write(sockfd, &p, sizeof(struct product));

                // Receive response from server and print it
                char response[100];
                read(sockfd, response, sizeof(response));
                printf("%s", response);
            }

            else if (i == 6){

                // Get the customer ID from user input
                
                int cusid;
                printf("Enter your customerID: ");
                scanf("%d",&cusid);
                write(sockfd, &cusid, sizeof(int));

                // Wait for the server to verify the customer ID
                int is_valid_customer;
                read(sockfd, &is_valid_customer, sizeof(int));
                if (is_valid_customer == -1) {
                    printf("Invalid customer ID. Please try again.\n");
                    continue;
                }


                // Read the contents of the user's shopping cart
                struct cart c;
                read(sockfd, &c, sizeof(struct cart));

                // For each product in the cart, display its details (ordered quantity, stock quantity, price)
                int ordered, received, price;
                for (int i=0; i<MAX_PROD; i++){
                    if (c.products[i].id != -1){
                        read(sockfd, &ordered, sizeof(int));
                        read(sockfd, &received, sizeof(int));
                        read(sockfd, &price, sizeof(int));
                        printf("Product ID: %d\n", c.products[i].id);
                        printf("Ordered: %d; Received: %d; Price: %d\n", ordered, received, price);
                        c.products[i].qty = received;
                        c.products[i].price = price;
                    }
                }

                // Calculate the total price of the cart
                int total = 0;
                for (int i=0; i<MAX_PROD; i++){
                    if (c.products[i].id != -1){
                        total += c.products[i].qty * c.products[i].price;
                    }
                }

                printf("Total amount to be paid is: %d\n",total);

                // Ask the user to enter the payment amount and verify that it matches the total
                int payment;
                while (1){
                    printf("Enter the payment amount: ");
                    scanf("%d", &payment);
                    if (payment != total){
                        printf("Please enter the correct amount!\n");
                    }else{
                        break;
                    }
                }
                
                write(sockfd, &payment, sizeof(int));
                write(sockfd, &total, sizeof(int));
                write(sockfd, &c, sizeof(struct cart));
                
                printf("Payment successful. Check Receipt!\n");

                printf("\n╔═════════════════════════╗\n");
                printf("║          RECEIPT        ║\n");
                printf("╠═════════════════════════╣\n");
                printf("║ Total:%-18d║\n", total);
                printf("╠════════════╦════════════╣═════════║\n");
                printf("║ Product ID ║  Quantity  ║  Price  ║\n");
                printf("╠════════════╬════════════╬═════════╣\n");

                for (int i = 0; i < MAX_PROD; i++) {
                    if(c.products[i].id==-1){
                        break;
                    }
                    printf("║ %-10d ║ %-10d ║ $%-6d ║\n", c.products[i].id, c.products[i].qty, c.products[i].price);
                }

                printf("╚════════════╩════════════╩═════════╝\n");


            }

            else if (i == 7){

                int id;
                read(sockfd, &id, sizeof(int));
                printf("Your registered customer ID: %d\n", id);
                    
            }

            else{
                printf("Please enter a valid choice. Try again!\n");
            }

            
        }
    }
    else if (user == 2){
        
        while (1){

            // Header
            printf("============================================\n");
            printf("|                INVENTORY MENU             |\n");
            printf("============================================\n");

            // Menu options
            printf("| 1. Add a product                           |\n");
            printf("| 2. Delete a product                        |\n");
            printf("| 3. Update price of a product               |\n");
            printf("| 4. Update quantity of a product            |\n");
            printf("| 5. View your inventory                     |\n");
            printf("| 6. Exit                                    |\n");

            // Prompt for input
            printf("|-------------------------------------------|\n");
            printf("| Please enter a number to select an option: |\n");
            printf("============================================\n");


            // char ch;
            // scanf("%c",&ch);
            // scanf("%c",&ch);

            int i;
            scanf("%i",&i);
            write(sockfd, &i, sizeof(int));

            if (i == 1){
                //add a product

                int id, qty, price;
                char name[50];

                printf("Enter name of the product to add\n");
                scanf("%s", name);
                id = productInput();
                qty = qtyInput();
                price = priceInput();
                
                struct product p;
                p.id = id;
                strcpy(p.name, name);
                p.qty = qty;
                p.price = price;

                write(sockfd, &p, sizeof(struct product));

                char response[100];
                read(sockfd, response, sizeof(response));

                printf("%s", response);

            }

            else if (i == 2){

                printf("Enter product: ");
                int id = productInput();
                
                write(sockfd, &id, sizeof(int));
                //deleting is equivalent to setting everything as -1

                char response[100];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);
            }

            else if (i == 3){
                int id = productInput();

                int price = priceInput();
                
                struct product p;
                p.id = id;
                p.price = price;
                write(sockfd, &p, sizeof(struct product));

                char response[100];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);
            }

            else if (i == 4){
                int id = productInput();
                int qty = qtyInput();

                struct product p;
                p.id = id;
                p.qty = qty;
                write(sockfd, &p, sizeof(struct product));

                char response[100];
                read(sockfd, response, sizeof(response));
                printf("%s\n", response);

            }

            else if (i == 5){
                 // Print table header
                printf("====================================================================\n");
                printf("| Product ID | Product Name        | Quantity in Stock | Price |\n");
                printf("====================================================================\n");

                // Read and print each product until the end of file is reached
                while (1){
                    struct product p;
                    read(sockfd, &p, sizeof(struct product));

                    if (p.id != -1 && p.qty > 0){
                        printf("| %10d | %20s | %17d | %9d |\n", p.id, p.name, p.qty, p.price);
                    } else {
                        // End of file reached
                        break;
                    }
                }

                // Print table footer
                printf("====================================================================\n");

            }

            else if (i == 6){
                break;
            }

            else{
                printf("Please enter a valid choice. Try again!\n");
            }
        }
    }

    printf("Program exited\n");
    close(sockfd);
    return 0;
}