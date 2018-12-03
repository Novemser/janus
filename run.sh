#!/bin/bash
for ((i=1;i<=20;i++));
do
echo '  n{$i}: localhost' >> hosts-local.yml
done
