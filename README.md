# nginx-snowflake-module

==================

Snowflake is a distributed unique ID generator inspired by [Twitter's Snowflake](https://blog.twitter.com/2010/announcing-snowflake).  
Now changed snowflake ID is composed of

    32 bits for time in units of second
    8 bits for a group id
    6 bits for a work id
    17 bits for a sequence number
    
    
    
 ```shell
    $ cat error.log
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake time_max: "2147483647" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake seq_max: "131071" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake seq: "0" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake time: "0" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake worker_id: "52" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake worker_id_max: "63" in /usr/local/nginx/conf/nginx.conf:142
2019/04/14 11:57:37 [notice] 56374#0: ngx_snowflake group_id_mx: "255" in /usr/local/nginx/conf/nginx.conf:142
    
 ```
 
 
 
### Features:

- time can support more than 68 years.
- support nginx multi-processes, not more than 64.  
- every process sequence max is 131071 in one second.
- each group can be one machine. max is 256.
- fast
- json

### Example configuration
---------------------

```
location = /test {
    snowflake_group_id 12;
}
```

``` shell
$ curl http://127.0.0.1/test
{"id":19215840550191104}

```

snowflake_group_id is machine unique id.


### Install
-------

Specify `--add-module=/path/to/nginx-snowflake-module` when you run `./configure`.

Example:

```
./configure --add-module=/path/to/nginx-snowflake-module
make
make install
```

If you want to add this module as a dynamic module, specify `--add-dynamic-module=/path/to/nginx-snowflake-module` instead.


### Q&A
-------

* email: huayulei_2003@hotmail.com
* QQ: 290692402
=======
snowflake uuid
