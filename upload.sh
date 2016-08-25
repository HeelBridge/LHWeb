#!/bin/bash

echo -n "IP-Adresse: "
read ip

curl -i -F "file=@w3.css;filename=/w3.css" "$ip/upload"
curl -i -F "file=@index.tmpl;filename=/index.tmpl" "$ip/upload"
curl -i -F "file=@webconfig.tmpl;filename=/webconfig.tmpl" "$ip/upload"
curl -i -F "file=@browse.tmpl;filename=/browse.tmpl" "$ip/upload"
curl -i -F "file=@log.tmpl;filename=/log.tmpl" "$ip/upload"
curl -i -F "file=@userconfig.tmpl;filename=/userconfig.tmpl" "$ip/upload"

