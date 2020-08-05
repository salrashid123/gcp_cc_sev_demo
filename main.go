// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"bufio"
	"fmt"
	"os"
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

const (
	SET_C_BIT   = 1
	CLEAR_C_BIT = 0
)

var ()

func main() {
	fd, err := unix.Open("/dev/sevtest", syscall.O_RDONLY, 0)
	if err != nil {
		panic(err)
	}
	defer unix.Close(fd)

	reader := bufio.NewReader(os.Stdin)
	fmt.Print("Enter text: ")
	secret, _ := reader.ReadString('\n')

	i := fmt.Sprintf("%s", secret)

	fmt.Println("c-bit set to default(1): ", &i, " Value : ", i)

	loc := int(uintptr(unsafe.Pointer(syscall.StringBytePtr(i))))
	fmt.Println("------------------------------")

	err = unix.IoctlSetInt(fd, CLEAR_C_BIT, loc)
	if err != nil {
		panic(err)
	}
	fmt.Println("c-bit set to 0; Address: ", &i, " Value : ", i)
	fmt.Println("------------------------------")

	err = unix.IoctlSetInt(fd, SET_C_BIT, loc)
	if err != nil {
		panic(err)
	}
	fmt.Println("c-bit set to 1; Address: ", &i, " Value : ", i)
}
