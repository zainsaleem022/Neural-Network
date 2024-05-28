#include <iostream>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <sys/wait.h>
using namespace std;

const int No_Layers = 6;
const int No_Nodes = 8;
double x1=0;
double x2=0;
int fd[No_Layers][No_Nodes][2];
pthread_mutex_t InMutex;
pthread_mutex_t* mutexarray;
int functioning_layer_id = -1;
int inputcompleted = 0;
int secondinputcompleted = 0;
int neuroninputcounter = 0;
int current_layer_pipe[2];
double final_output_first_forward_propagation = 0;
int backpropagationpipe[2];                     //shared pipe to determine either forward propagation or backwards
                                                //-1 for backwards and 1 for forward propagation
int secondforwardpropipe[2];
int indicatelayersfrontprop[2];
bool secondforwardpropdone = false;

struct neuron{
    int i;
    int j;
};

struct InputStruct
{
    double Input;
    int number;
};

void* InputThreadsFunction(void* args)
{
    InputStruct* n= (InputStruct*)args;

againforwardprop:
    double againval = 0;
    int secondfrontprop = 0;
    double InArr[2][No_Nodes]{};
    double output = 0;

    pthread_mutex_lock(&InMutex);
    ifstream reader("InputWeights.txt");
    string word;

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < No_Nodes; j++)
        {
            reader >> word;
            InArr[i][j] = stod(word);
        }
    }
    
    reader.close();


    for(int i=0; i<No_Nodes; i++)
    {
        output = n->Input*InArr[n->number][i];
        write(fd[0][i][1], &output, sizeof(output));
        cout<<"sending to "<<i<<" is val: "<<output<<endl;
    }

    inputcompleted++;

    if(secondforwardpropdone == true)
    {
        secondinputcompleted++;
    }

    if(inputcompleted == 2 || secondinputcompleted == 2)
    {
        int value = 0;
        write(current_layer_pipe[1], &value, sizeof(value));
    }

    
    pthread_mutex_unlock(&InMutex);
    
    while(secondforwardpropdone);
    while(secondfrontprop != 1)
    {
        pthread_mutex_lock(&InMutex);
        read(secondforwardpropipe[0], &secondfrontprop, sizeof(secondfrontprop));
        write(secondforwardpropipe[1], &secondfrontprop, sizeof(secondfrontprop));
        pthread_mutex_unlock(&InMutex);
    }
    cout<<"Second farward prop started"<<endl;
    pthread_mutex_lock(&InMutex);
    secondforwardpropdone = true;
    read(fd[0][(No_Nodes-1)][0], &againval, sizeof(againval));
    secondfrontprop = 1;
    write(indicatelayersfrontprop[1], &secondfrontprop, sizeof(secondfrontprop));
    //cout<<againval<<endl;
    n->Input = againval;
    pthread_mutex_unlock(&InMutex);
    goto againforwardprop;
}




void* thread_function(void* args)
{

    
    neuron* n = (neuron*)args;
    int secondfrontpropvalue = 0;
    int backcurrentlayer = -1;
    int checkbackpropvalue;
    int assignbacklayer;
    double getx1,getx2;
    int current_layer = -1;
    int backpropvalue = 0;
    pthread_mutex_lock(&mutexarray[n->i]);
    read(backpropagationpipe[0], &backpropvalue, sizeof(backpropvalue));
    write(backpropagationpipe[1], &backpropvalue, sizeof(backpropvalue));
    //cout<<"Backpropvalue: "<<backpropvalue<<endl;
    pthread_mutex_unlock(&mutexarray[n->i]);


reloop:


    backcurrentlayer = -1;
    current_layer = -1;

    if(backpropvalue == 0 || backpropvalue == 3)
    {

        while(neuroninputcounter != n->j);                  //busy loop condition for the running threads in increasing order from 0 to no of neurons
        while (current_layer != n->i)                       //busy loop condition for running the layers one by one in an increasing order
        {
            pthread_mutex_lock(&mutexarray[n->i]);
            read(current_layer_pipe[0], &current_layer, sizeof(current_layer));
            write(current_layer_pipe[1], &current_layer, sizeof(current_layer));
            // if(backpropvalue == 3)
            // {
            //     cout<<"Layernumber: "<<current_layer<<endl;
            // }
            pthread_mutex_unlock(&mutexarray[n->i]);
        }


        pthread_mutex_lock(&mutexarray[n->i]);              //condition for simple file name and output weights for the final output layer
        string filename;
        filename = to_string((n->i)+1) + ".txt";
        if((n->i)+1 != No_Layers)
        {
            filename = to_string((n->i)+1) + ".txt";
        }
        else
        {
            filename = "OutputWeights.txt";
        }
        ifstream reader;
        reader.open(filename);
        double finalans = 0;
        double array[No_Nodes][No_Nodes];
        string word;
        double outputweight;

                                            //condition for opening the files and making an array of those neuron to neuron connection weights
                                            //condition for opening the output weights and skip till its neuron number weight doesnt come

        if((n->i)+1 != No_Layers)
        {
            for (int i = 0; i < No_Nodes; i++)
            {
                for (int j = 0; j < No_Nodes; j++)
                {
                    reader >> word;
                    array[i][j] = stod(word);
                }
            }

            reader.close();
        }

        else
        {
            for(int i=0; i<=n->j; i++)
            {
                reader>>word;
            }
            outputweight = stod(word);
            //cout<<"Layer Number "<<n->i<<" and thread number "<<n->j<<" has extracted the value: "<<outputweight<<endl;
            reader.close();
        }


        if(n->i == 0)                           //conditions for reading from the pipes and adding all the values
        {
            for(int i=0; i<2; i++)
            {
                double frompipe;
                read(fd[n->i][n->j][0], &frompipe, sizeof(frompipe));
                //cout<<"Layer Number: "<<n->i<<" and Thread Number: "<<n->j<<" has recieved: "<<frompipe<<endl;
                finalans += frompipe;
            }
        }
        else
        {
            for(int i=0; i<No_Nodes; i++)
            {
                double frompipe;
                read(fd[n->i][n->j][0], &frompipe, sizeof(frompipe));
                //cout<<"Layer Number: "<<n->i<<" and Thread Number: "<<n->j<<" has recieved: "<<frompipe<<endl;
                finalans += frompipe;
            }
        }

        //cout<<endl;

        neuroninputcounter++;


        double nextweight = 0;
        if(n->i == (No_Layers-1))                               //condition for the last output layer multiplying with its weights
        {
            nextweight = finalans*outputweight;
            final_output_first_forward_propagation += nextweight;
            cout<<"Layer Number: "<<n->i<<" and Thread Number: "<<n->j<<" is sending: "<<nextweight<<endl;
            //cout<<" with final weight: "<<final_output_first_forward_propagation<<endl;
        }
        else
        {
            for(int i=0; i<No_Nodes; i++)                     //condition for all the hidden layers multiplying weights and writing into pipes
            {
                nextweight = array[n->j][i]*finalans;
                write(fd[(n->i)+1][i][1], &nextweight, sizeof(nextweight));
                cout<<"Layer Number: "<<n->i<<" and Thread Number: "<<n->j<<" is sending: "<<nextweight<<endl;
            }
        }
        //cout<<endl<<endl<<endl;

        
        cout<<neuroninputcounter<<endl;
        if(neuroninputcounter == No_Nodes)                  //current functioning layer condition
        {
            if(n->i == (No_Layers-1))
            {
                if(backpropvalue == 0)
                {
                    cout<<"First Forward Propagation Done"<<endl;
                    cout<<"First Propagation Ans: "<<final_output_first_forward_propagation<<endl;
                }
                else
                {
                    cout<<"Second Forward Propagation Done"<<endl;
                    cout<<"Second Propagation Ans: "<<final_output_first_forward_propagation<<endl;
                }

                x1 = ((((final_output_first_forward_propagation)*(final_output_first_forward_propagation))+(final_output_first_forward_propagation))+1)/2;
                x2 = (((final_output_first_forward_propagation)*(final_output_first_forward_propagation))-(final_output_first_forward_propagation))/2;
                cout<<"X1: "<<x1<<"\tX2: "<<x2<<endl;

                //writing the x1 and x2 from first propagation into the first neuron of each layer for backward propagation
                write(fd[n->i][n->j][1], &x1, sizeof(x1));
                write(fd[n->i][n->j][1], &x2, sizeof(x2));

                if(backpropvalue == 0)
                {
                    backpropvalue = 1;
                    write(backpropagationpipe[1], &backpropvalue, sizeof(backpropvalue));
                }
                else
                {
                    backpropvalue = 4;
                }
                pthread_mutex_unlock(&mutexarray[n->i]);
                goto reloop;
            }
            else                                            //else it increments the currently functioning layer number in the shared pipe 
            {
                read(current_layer_pipe[0], &current_layer, sizeof(current_layer));
                current_layer = (n->i)+1;
                write(current_layer_pipe[1], &current_layer, sizeof(current_layer));
            }
        }

        pthread_mutex_unlock(&mutexarray[n->i]);

        if(backpropvalue == 3)
        {
            while(1);
        }
    }

    if(backpropvalue == 2 || backpropvalue == 0 && n->j != (No_Nodes-1))        //all other neurons will be here waiting other than the last neuron of each array 
    {
        
        backpropvalue = 3;
        //cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
        while(secondfrontpropvalue != 1)
        {
            //cout<<"-";
            pthread_mutex_lock(&mutexarray[n->i]);
            //cout<<secondfrontpropvalue<<endl;
            read(indicatelayersfrontprop[0], &secondfrontpropvalue, sizeof(secondfrontpropvalue));
            write(indicatelayersfrontprop[1], &secondfrontpropvalue, sizeof(secondfrontpropvalue));
            pthread_mutex_unlock(&mutexarray[n->i]);
        }
        final_output_first_forward_propagation = 0;
        x1 = 0;
        x2 = 0;
        neuroninputcounter = 0;
        //cout<<"Layer number: "<<n->i<<" and thread number "<<n->j<<" is free"<<endl;
        //while(1);
        goto reloop;
    }

    else if(backpropvalue == 4)
    {
        while(1);
    }

    else if(backpropvalue == 1)
    {
        cout<<"Current_Layer inside back propagation: "<<n->i<<endl;
        //cout<<"+++Layer number: "<<n->i<<" and thread number: "<< n->j <<" is inside back propagation+++"<<endl;
        pthread_mutex_lock(&mutexarray[n->i]);
        //cout<<"hello"<<endl;
        read(fd[n->i][n->j][0], &getx1, sizeof(getx1));
        cout<<"Sending x1: "<<getx1<<endl;
        read(fd[n->i][n->j][0], &getx2, sizeof(getx2));
        cout<<"Sending x2: "<<getx2<<endl;
        if(n->i != 0)
        {
            write(fd[(n->i)-1][n->j][1], &getx1, sizeof(getx1));
            write(fd[(n->i)-1][n->j][1], &getx2, sizeof(getx2));
        }
        else
        {
            write(fd[(n->i)][n->j][1], &getx1, sizeof(getx1));
            write(fd[(n->i)][n->j][1], &getx2, sizeof(getx2));
        }

        read(current_layer_pipe[0], &assignbacklayer, sizeof(assignbacklayer));
        assignbacklayer -= 1;
        write(current_layer_pipe[1], &assignbacklayer, sizeof(assignbacklayer));
        backpropvalue = 2;
        if(n->i == 0)
        {
            int secondfrontprop = 1;
            write(secondforwardpropipe[1], &secondfrontprop, sizeof(secondfrontprop));
        }
        pthread_mutex_unlock(&mutexarray[n->i]);
        goto reloop;
    }


    while(checkbackpropvalue != 1)
    {
        pthread_mutex_lock(&mutexarray[n->i]);
        read(backpropagationpipe[0], &checkbackpropvalue, sizeof(checkbackpropvalue));
        write(backpropagationpipe[1], &checkbackpropvalue, sizeof(checkbackpropvalue));
        pthread_mutex_unlock(&mutexarray[n->i]);
    }

    while (backcurrentlayer != n->i)                       //busy loop condition for running the layers one by one in an increasing order
    {
        pthread_mutex_lock(&mutexarray[n->i]);
        read(current_layer_pipe[0], &backcurrentlayer, sizeof(backcurrentlayer));
        write(current_layer_pipe[1], &backcurrentlayer, sizeof(backcurrentlayer));
        pthread_mutex_unlock(&mutexarray[n->i]);
    }

    backpropvalue = 1;
    goto reloop;
}


int main()
{
    int flag = 0;
    pthread_mutex_init(&InMutex, NULL);
    mutexarray = new pthread_mutex_t[No_Layers];

    for(int i=0;i<No_Layers; i++)
    {
        pthread_mutex_init(&mutexarray[i], NULL);
    }

    ifstream file("file.txt");


    double Input[2];
    ofstream myfile("InputWeights.txt");

    char c;
    string s;
    string fin;
    bool flay = false;
    int val = 0;

    string word;
    while (val != No_Nodes*2 && file >> word)
    {
        if(flag == 0)
        {
            Input[0] = stod(word);
            flag++;
        }
        else if(flag == 1)
        {
            Input[1] = stod(word);
            flag++;
        }
        else if(flag == 2)
        {
            myfile << word << " ";
            val++;
        }

    }
    myfile.close();

    cout << endl;
    for(int i = 1; i <= No_Layers-1; i++)
    {
        string filename = to_string(i) + ".txt";
        ofstream layerfile(filename);

            for (int j = 0; j < No_Nodes*No_Nodes; j++)
            {
                file >> word;
                layerfile << word << " "; 
            }
        layerfile.close();
    }

    ofstream outputfile("OutputWeights.txt");
    while(file >> word)
    {
        outputfile << word << " ";
    }
    outputfile.close();

    file.close();



    for (int i = 0; i < No_Layers; i++)                                 //creating pipe for each neuron
    {
        for (int j = 0; j < No_Nodes; j++)
        {
            if (pipe(fd[i][j]) == -1) {
                cerr << "Failed to create pipe" << endl;
                return 1;
            }
        }
    }

    if(pipe(current_layer_pipe) == -1)                                  //making current functioning layer number holding pipe
    {
        std::cerr << "Failed to create pipe" << std::endl;
        return 1;
    }

    if(pipe(backpropagationpipe) == -1)                                 //writing backpropagation value as 0 and will be 1 when its happening
    {
        std::cerr << "Failed to create pipe" << std::endl;
        return 1;
    }

    if(pipe(indicatelayersfrontprop) == -1)                                 //writing backpropagation value as 0 and will be 1 when its happening
    {
        std::cerr << "Failed to create pipe" << std::endl;
        return 1;
    }

    


    if(pipe(secondforwardpropipe) == -1)                                 //writing backpropagation value as 0 and will be 1 when its happening
    {
        std::cerr << "Failed to create pipe" << std::endl;
        return 1;
    }

    int backpropvalue = 0;
    write(backpropagationpipe[1], &backpropvalue, sizeof(backpropvalue));
    write(secondforwardpropipe[1], &backpropvalue, sizeof(backpropvalue));
    write(indicatelayersfrontprop[1], &backpropvalue, sizeof(backpropvalue));


    pthread_t InputNeronThread;
    pthread_attr_t InputNeronThread_attr;

    InputStruct InStr[2];
    for (int i = 0; i < 2; i++)
    {
        pthread_attr_init(&InputNeronThread_attr);
        pthread_attr_setdetachstate(&InputNeronThread_attr, PTHREAD_CREATE_DETACHED);
        InStr[i].Input = Input[i];
        InStr[i].number = i;
        pthread_create(&InputNeronThread, &InputNeronThread_attr, InputThreadsFunction, &InStr[i]);
    }

    sleep(1);
    pthread_t* threads;
    int counter = 0;
    for(int i=0; i<No_Layers; i++)
    {
        pid_t pid;
        pid = fork();

        if(pid == 0)
        {
            threads = new pthread_t[No_Nodes];
            for(int j=0; j<No_Nodes; j++)
            {
                neuron* n = new neuron;
                n->i = i;
                n->j = j;
                pthread_create(&threads[j], NULL ,thread_function, n);
            }

            counter++;
            if(counter == No_Layers)
            {
                while(1);
            }
        }

        else
        {
            wait(NULL);
        }
    }


    for (int i = 0; i < No_Layers; i++)
    {
        for (int j = 0; j < No_Nodes; j++)
        {
            close(fd[i][j][0]);
            close(fd[i][j][1]);
        }
    }

    pthread_mutex_destroy(&InMutex);
    for(int i=0;i<No_Layers; i++)
    {
        pthread_mutex_destroy(&mutexarray[i]);
    }
    return 0;
}
