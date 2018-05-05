#!groovy

// Taken from https://github.com/owncloud/client/blob/master/Jenkinsfile

// Constructed from the DockerFile in admin/linux/DockerFile
//def linux = docker.image('dominikschmidt/docker-owncloud-client-linux:latest')
// Constructed from the DockerFile in admin/win/docker/DockerFile
//def win32 = docker.image('dominikschmidt/docker-owncloud-client-win32-cross:latest')

pipeline {
    agent {
        docker { image 'codelite:latest' }
    }
    stages {
        stage('Linux - GCC'){
            steps {
                sh '''
                mkdir -p ~/.ccache/tmp
                rm -rf build
                mkdir build
                cd build
                export PATH=/usr/lib/ccache:$PATH
                cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=DebugFull ..
                make -j6
                '''
            }
        }

    }
}
