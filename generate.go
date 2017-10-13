package main

//go:generate go run $GOROOT/src/syscall/mksyscall_windows.go -systemdll=false -output zsyscall.go forwarding.go
//go:generate msbuild /p:Configuration=Release forwarding\forwarding.vcxproj
