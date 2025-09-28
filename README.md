
<p align="center">
  <img src="meshcore.png" width=300px />
</p>

# Meshcore Vanity

This script mines vanity addresses for meshcore.

## Background

Meshcore uses Nightcracker's portable implementation of ed25519 to calculate signatures / encryption. For some reason, this implementation produces different results to standard python libraries such as libsodium or nacl, which is why we're forced to use C to calculate these vanity addresses.

## Features

 - Calculates vanity addresses on the CPU (multithreaded 👀)

## Caveats

Because this script calculates vanity addresses on the CPU and not the GPU, it's quite slow. For something like 'deadbeef' it took me around 45 minutes on 16 threads. For every character you add on, this number is multiplied 16-fold, so chose wisely and be patient.

## Usage

See releases to download the binary (linux only!):

```bash
$ ./meshcore_vanity B00B1E5 16
Started looking for 'B00B1E5' using 16 threads...
Found vanity public key after 16566534 attempts!
Identity: fbe10d...b00b1e5...68fc4b
```

In my case, I sent the identity key to my phone via email (i'm lazy ok), then loaded it via the meshcore companion app.

Note that the identity contains the private key & public key concatenated together; once it's loaded in meshcore the vanity text will appear at the beginning of the public key (see image above).

## Building

```
make
```