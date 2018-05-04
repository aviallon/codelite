#!groovy

// Taken from https://github.com/owncloud/client/blob/master/Jenkinsfile

// Constructed from the DockerFile in admin/linux/DockerFile
//def linux = docker.image('dominikschmidt/docker-owncloud-client-linux:latest')
// Constructed from the DockerFile in admin/win/docker/DockerFile
//def win32 = docker.image('dominikschmidt/docker-owncloud-client-win32-cross:latest')

pipeline {
    agent {
        docker { image 'aviallon/ubuntu16.04:codelite' }
    }
    stages {
        stage('Linux - GCC'){
            steps {
                sh '''
                rm -rf build
                mkdir build
                cd build
                cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=DebugFull ..
                make -j4
                '''
            }
        }

    }
}
