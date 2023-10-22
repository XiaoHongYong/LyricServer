#!/bin/bash

certbot renew --force-renewal
/usr/sbin/service openresty reload
