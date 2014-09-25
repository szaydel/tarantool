:title: IProto protocol
:slug: box-protocol-new
:save_as: doc/new-box-protocol.html
:url: doc/new-box-protocol.html
:template: documentation

--------------------------------------------------------------------------------
                                    Overview
--------------------------------------------------------------------------------

IPROTO is a binary request/response protocol. The server begins the dialogue by
sending a fixed-size (128 bytes) text greeting to the client. The first 64 bytes
of the greeting contain server version. The second 64 bytes contain a
base64-encoded random string, to use in authentification packet.

Once a greeting is read, the protocol becomes pure request/response and features
a complete access to Tarantool functionality, including:

- request multiplexing, e.g. ability to asynchronously issue multiple requests\
  via the same connection
- response format that supports zero-copy writes

For data structuring and encoding, the protocol uses msgpack data format, see
http://msgpack.org

Tarantool protocol mandates use of a few integer constants serving as keys in maps
used in the protocol. These constants are defined in
https://github.com/tarantool/tarantool/blob/master/src/iproto_constants.h

Let's list them here too:

.. code-block:: lua

    -- user keys
    <code>          ::= 0x00
    <sync>          ::= 0x01
    <space_id>      ::= 0x10
    <index_id>      ::= 0x11
    <limit>         ::= 0x12
    <offset>        ::= 0x13
    <iterator>      ::= 0x14
    <key>           ::= 0x20
    <tuple>         ::= 0x21
    <function_name> ::= 0x22
    <username>      ::= 0x23
    <data>          ::= 0x30
    <error>         ::= 0x31

.. code-block:: lua

    -- -- Value for <code> key in request can be:
    -- User command codes
    0x01 -- <select>
    0x02 -- <insert>
    0x03 -- <replace>
    0x04 -- <update>
    0x05 -- <delete>
    0x06 -- <call>
    0x07 -- <auth>
    -- Admin command codes
    0x40 -- <ping>

    -- -- Value for <code> key in response can be:
    0x00   -- <OK>
    0x8XXX -- <ERROR>


--------------------------------------------------------------------------------
                         Unified package structure
--------------------------------------------------------------------------------

.. code-block:: lua

    REQUEST:

    0      5
    +------+ +============+ +===================================+
    | BODY | |            | |                                   |
    |HEADER| |   HEADER   | |               BODY                |
    | SIZE | |            | |                                   |
    +------+ +============+ +===================================+
     MP_INT      MP_MAP                     MP_MAP


.. code-block:: lua

    UNIFIED HEADER:

    +================+================+
    |                |                |
    |   0x00: CODE   |   0x01: SYNC   |
    | MP_INT: MP_INT | MP_INT: MP_INT |
    |                |                |
    +================+================+
                   MP_MAP

--------------------------------------------------------------------------------
                         Greeting Package
--------------------------------------------------------------------------------

.. code-block:: lua

    TARANTOOL'S GRETTING:

    0                                     63
    +--------------------------------------+
    |                                      |
    | Tarantool Greeting (server version)  |
    |               64 bytes               |
    +---------------------+----------------+
    |                     |                |
    | BASE64 encoded SALT |      NULL      |
    |      44 bytes       |                |
    +---------------------+----------------+
    64                  107              127

--------------------------------------------------------------------------------
                         Request packet structure
--------------------------------------------------------------------------------

.. code-block:: lua

    PREPARE SCRAMBLE:

        LEN(ENCODED_SALT) = 44;
        LEN(SCRAMBLE)     = 20;

    prepare 'chap-sha1' scramble:

        salt = base64_decode(encoded_salt);
        step_1 = sha1(password);
        step_2 = sha1(step_1);
        step_3 = sha1(salt, step_2);
        scramble = xor(step_1, step_4);
        return scramble;

    AUTHORIZATION BODY: CODE = 0x07

    +==================+====================================+
    |                  |        +-------------+-----------+ |
    |                  | (TUPLE)|  len == 9   | len == 20 | |
    |   0x23:USERNAME  |   0x21:| "chap-sha1" |  SCRAMBLE | |
    | MP_INT:MP_STRING | MP_INT:|  MP_STRING  | MP_STRING | |
    |                  |        +-------------+-----------+ |
    |                  |                   MP_ARRAY         |
    +==================+====================================+
                            MP_MAP


.. code-block:: lua

    REPLACE: CODE - 0x03
    Insert a tuple into the space or replace an existing one.
    INSERT:  CODE - 0x02
    Insert is similar to replace, but will return a duplicate
    key error if such tuple already exists.

    INSERT/REPLACE BODY:

    +==================+==================+
    |                  |                  |
    |   0x10: SPACE_ID |   0x21: TUPLE    |
    | MP_INT: MP_INT   | MP_INT: MP_ARRAY |
    |                  |                  |
    +==================+==================+
                     MP_MAP

Find tuples matching the search pattern

.. code-block:: lua

    SELECT: CODE - 0x01
    Find tuples matching the search pattern

    SELECT BODY:

    +==================+==================+==================+
    |                  |                  |                  |
    |   0x10: SPACE_ID |   0x11: INDEX_ID |   0x12: LIMIT    |
    | MP_INT: MP_INT   | MP_INT: MP_INT   | MP_INT: MP_INT   |
    |                  |                  |                  |
    +==================+==================+==================+
    |                  |                  |                  |
    |   0x13: OFFSET   |   0x14: ITERATOR |   0x14: KEY      |
    | MP_INT: MP_INT   | MP_INT: MP_INT   | MP_INT: MP_ARRAY |
    |                  |                  |                  |
    +==================+==================+==================+
                              MP_MAP

.. code-block:: lua

    Delete a tuple
    DELETE BODY:

    +==================+==================+==================+
    |                  |                  |                  |
    |   0x10: SPACE_ID |   0x11: INDEX_ID |   0x14: KEY      |
    | MP_INT: MP_INT   | MP_INT: MP_INT   | MP_INT: MP_ARRAY |
    |                  |                  |                  |
    +==================+==================+==================+
                              MP_MAP

.. code-block:: lua

    Update a tuple
    UPDATE BODY:

    +==================+==================+==================+=======================+
    |                  |                  |                  |          +~~~~~~~~~~+ |
    |                  |                  |                  |          |          | |
    |                  |                  |                  | (TUPLE)  |    OP    | |
    |   0x10: SPACE_ID |   0x11: INDEX_ID |   0x14: KEY      |    0x21: |          | |
    | MP_INT: MP_INT   | MP_INT: MP_INT   | MP_INT: MP_ARRAY |  MP_INT: +~~~~~~~~~~+ |
    |                  |                  |                  |            MP_ARRAY   |
    +==================+==================+==================+=======================+
                                       MP_MAP

    OP:
        Works only for INTEGERS
        * Addition    OP = '+' - space[key][field_no] += argument
        * Subtraction OP = '-' - space[key][field_no] -= argument
        * Bitwise AND OP = '&' - space[key][field_no] &= argument
        * Bitwise XOR OP = '^' - space[key][field_no] ^= argument
        * Bitwise OR  OP = '|' - space[key][field_no] |= argument
        * Delete      OP = '#'
          delete <argument> fields starting from <field_no> in the space[<key>]

    +-----------+==========+==========+
    |           |          |          |
    |    OP     | FIELD_NO | ARGUMENT |
    | MP_FIXSTR |  MP_INT  |  MP_INT  |
    |           |          |          |
    +-----------+==========+==========+
                  MP_ARRAY

        * Insert      OP = '!'
          insert <argument> before <field_no>
        * Assign      OP = '='
          assign <argument> to field <field_no>.
          will extend the tuple if <field_no> == <max_field_no> + 1

    +-----------+==========+===========+
    |           |          |           |
    |    OP     | FIELD_NO | ARGUMENT  |
    | MP_FIXSTR |  MP_INT  | MP_OBJECT |
    |           |          |           |
    +-----------+==========+===========+
                  MP_ARRAY

        * Splice      OP = ':'
          take the string from space[key][field_no] and
          substitute <offset> bytes from <position> with <argument>

    +-----------+==========+==========+========+==========+
    |           |          |          |        |          |
    |    ':'    | FIELD_NO | POSITION | OFFSET | ARGUMENT |
    | MP_FIXSTR |  MP_INT  |  MP_INT  | MP_INT |  MP_STR  |
    |           |          |          |        |          |
    +-----------+==========+==========+========+==========+
                             MP_ARRAY

.. code-block:: lua

    Call a stored function
    CALL BODY:

    +=======================+==================+
    |                       |                  |
    |   0x22: FUNCTION_NAME |   0x21: TUPLE    |
    | MP_INT: MP_STRING     | MP_INT: MP_ARRAY |
    |                       |                  |
    +=======================+==================+
                        MP_MAP

--------------------------------------------------------------------------------
                         Response packet structure
--------------------------------------------------------------------------------

.. code-block:: lua

    We'll show whole packet here:

    OK:    LEN + HEADER + BODY

    0      5                                          OPTIONAL
    +------++================+================++===================+
    |      ||                |                ||                   |
    | BODY ||   0x00: 0x00   |   0x01: SYNC   ||   0x30: DATA      |
    |HEADER|| MP_INT: MP_INT | MP_INT: MP_INT || MP_INT: MP_OBJECT |
    | SIZE ||                |                ||                   |
    +------++================+================++===================+
     MP_INT                MP_MAP                      MP_MAP

    ERROR: LEN + HEADER + BODY

    0      5
    +------++================+================++===================+
    |      ||                |                ||                   |
    | BODY ||   0x00: 0x8XXX |   0x01: SYNC   ||   0x31: ERROR     |
    |HEADER|| MP_INT: MP_INT | MP_INT: MP_INT || MP_INT: MP_STRING |
    | SIZE ||                |                ||                   |
    +------++================+================++===================+
     MP_INT                MP_MAP                      MP_MAP

    Where 0xXXX is ERRCODE.

--------------------------------------------------------------------------------
                         Replication packet structure
--------------------------------------------------------------------------------

.. code-block:: lua

    -- replication keys
    <server_id>     ::= 0x02
    <lsn>           ::= 0x03
    <timestamp>     ::= 0x04
    <server_uuid>   ::= 0x24
    <cluster_uuid>  ::= 0x25
    <vclock>        ::= 0x26

.. code-block:: lua

    -- replication codes
    0x41 -- <join>
    0x42 -- <subscribe>


.. code-block:: lua

    JOIN:

    In the beggining you must send JOIN
                             HEADER                          BODY
    +================+================+===================++-------+
    |                |                |    SERVER_UUID    ||       |
    |   0x00: 0x41   |   0x01: SYNC   |   0x24: UUID      || EMPTY |
    | MP_INT: MP_INT | MP_INT: MP_INT | MP_INT: MP_STRING ||       |
    |                |                |                   ||       |
    +================+================+===================++-------+
                   MP_MAP                                   MP_MAP

    Then server, which we connect to, will send last SNAP file by, simply,
    creating a number of INSERT's (with additional LSN and ServerID) (don't reply)
    Then it'll send a vclock's MP_MAP and close a socket.

    +================+================++============================+
    |                |                ||        +~~~~~~~~~~~~~~~~~+ |
    |                |                ||        |                 | |
    |   0x00: 0x00   |   0x01: SYNC   ||   0x26:| SRV_ID: SRV_LSN | |
    | MP_INT: MP_INT | MP_INT: MP_INT || MP_INT:| MP_INT: MP_INT  | |
    |                |                ||        +~~~~~~~~~~~~~~~~~+ |
    |                |                ||                   MP_MAP   |
    +================+================++============================+
                   MP_MAP                      MP_MAP

    SUBSCRIBE:

    Then you must send SUBSCRIBE:

                                  HEADER
    +================+================+===================+===================+
    |                |                |    SERVER_UUID    |    CLUSTER_UUID   |
    |   0x00: 0x41   |   0x01: SYNC   |   0x24: UUID      |   0x25: UUID      |
    | MP_INT: MP_INT | MP_INT: MP_INT | MP_INT: MP_STRING | MP_INT: MP_STRING |
    |                |                |                   |                   |
    +================+================+===================+===================+
                                    MP_MAP
          BODY
    +================+
    |                |
    |   0x26: VCLOCK |
    | MP_INT: MP_INT |
    |                |
    +================+
          MP_MAP

    Then you must process every query that'll came through other masters.
    Every request between masters will have Additional LSN and SERVER_ID.

--------------------------------------------------------------------------------
                                XLOG / SNAP
--------------------------------------------------------------------------------

XLOG and SNAP have one format now. For example, they starts with:

.. code-block:: lua

    SNAP\n
    0.12\n
    Server: e6eda543-eda7-4a82-8bf4-7ddd442a9275\n
    VClock: {1: 0}\n
    \n
    ...

So, **Header** of SNAP/XLOG consists from:

.. code-block:: lua

    <format>\n
    <format_version>\n
    Server: <server_uuid>
    VClock: <vclock_map>\n
    \n


There're two markers: tuple beggining - **0xd5ba0bab** and EOF marker - **0xd510aded**. So, next, between **Header** and EOF marker there's data with such schema:

.. code-block:: lua

    0            3 4                                         17
    +-------------+========+============+===========+=========+
    |             |        |            |           |         |
    | 0xd5ba0bab  | LENGTH | CRC32 PREV | CRC32 CUR | PADDING |
    |             |        |            |           |         |
    +-------------+========+============+===========+=========+
      MP_FIXEXT2    MP_INT     MP_INT       MP_INT      ---

    +============+ +===================================+
    |            | |                                   |
    |   HEADER   | |                BODY               |
    |            | |                                   |
    +============+ +===================================+
        MP_MAP                     MP_MAP
