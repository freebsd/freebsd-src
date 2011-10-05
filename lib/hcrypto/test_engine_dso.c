/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>

#include <roken.h>
#include <getarg.h>

#include <engine.h>
#include <evp.h>

struct {
    const char *cpriv;
    const char *cpub;
    const char *spriv;
    const char *spub;
} dhtests[] = {
    {
	"5C0946275D07223AEAF04301D964498F3285946057B4C50D13B4FE12C88DFD8D499DD3CC00C1BC17C0D343F2FE053C9F53389110551715B1EDF261A0314485C4835D01F7B8894027D534A2D81D63619D2F58C9864AC9816086B3FF75C01B3FAFF355425AB7369A6ABDC8B633F0A0DC4D29B50F364E7594B297183D14E5CDC05D",
	"2D66DC5998B7AEE3332DC1061C6E6F6CF0FCCD74534187E2CDC9ACBCADF0FC9D5900451F44832A762F01E9CEEF1CBD7D69D020AC524D09FAD087DFADEAC36C845157B83937B51C8DB7F500C3C54FB2A05E074E40BA982186E7FEB2534EDDB387D5480AAA355B398CCAD0886F3952C3718490B7884FA67BD8B6943CDDA20134C6",
	"42644BA7CF74689E18BA72BF80FCA674D1A2ADF81795EB3828E67C30E42ABD07A8E90E27F046189FAC122D915276870B72427388EAAB5D06994FC38885BBACCEA1CFC45951B730D73C1A8F83208CD1351746601648C11D70BC95B817C86E4A5C40D633654615041C7934BB3CAF4E02754D542033DB024E94C7E561A29ED0C6EC",
	"C233633AB116E2DB20B4E08DA42DE8766293E6D9042F7A2C2A2F34F18FE66010B074CCF3C9B03EF27B14F0746B738AF22776224161D767D96AEC230A1DFA6DECFFCE9FED23B96F50CCB0093E59817AD0CEAEB7993AB5764679948BFB1293C9560B07AA3DFA229E341EB17C9FAE0B1D483082461D2DDBCEEE6FE7C0A34D96F66D"
    },
    {
	"76295C1280B890970F0F7EB01BBD9C5DF9BB8F590EB384A39EBF85CD141451407F955FD1D39012AA1F8BA53FD6A5A37CB2835CEDB27D1EBF1FE8AC9F2FFD628BD9BF7B8DD77CB80C8DC0A75F4567C7700442B26972833EB9738A8728A1FC274C59CED5E3ADA224B46711112AAA1CB831D2D6125E183ADA4F805A05024C9C6DDB",
	"1E0AB5EBAAC7985FE67A574447FAE58AE4CB95416278D4C239A789D4532FA8E6F82BA10BE411D8A0A06B9E1DECE704466B3523496A8A4165B97FBCFB9CE9C4FF2DEEE786BA046E8C270FA8A9055D2F6E42EDDB32C73CF7875551A56EB69C0F14A3745745845B81C347401B27D074C60C5177BA9C14BBB1C8C219B78E15126EF8",
	"68D84A8F92082F113542CFD990DEEFAD9C7EFA545268F8B3EBDF4CCBAF2865CF03EF60044EB4AF4154E6804CC2BDD673B801507446CEFC692DA577B6DC6E0272B7B081A1BEFDC2A4FAC83DB8845E3DA0D1B64DB33AA2164FEDB08A01E815336BD58F4E6DE6A265468E61C8C988B8AEC0D52DB714448DDC007E7C3382C07357DB",
	"393815D507A2EF80DE2D0F2A55AAB1C25B870ACA3FC97438B4336CBF979BF9A4F8DA1B61C667129F9123045E07E24976040EC5E2368DD4EF70690102D74E900B260D3826256FD473733A7569BF514652AB78C48C334FDCA26C44ABF322643AF15BFF693A37BB2C19CA9FE5F1537FCFE2B24CF74D4E57060D35ABF115B4B6CD21"
    },
    {
	"7307D6C3CB874327A95F7A6A91C336CEAA086736525DF3F8EC49497CF444C68D264EB70CD6904FE56E240EEF34E6C5177911C478A7F250A0F54183BCBE64B42BAB5D019E73E2F17C095C211E4815E6BA5FDD72786AF987ABBC9109ECEEF439AF9E2141D5222CE7DC0152D8E9A6CCCE301D21A7D1D6ACB9B91B5E28379C91890D",
	"83FBD7BFFDF415BBB7E21D399CB2F36A61AFDBAFC542E428E444C66AA03617C0C55C639FE2428905B57035892AE1BD2C4060E807D9E003B0C204FFC8FDD69CC8ADE7A8E18DCBFFF64E3EF9DA2C117390374241466E48A020A1B2F575AE42C233F8BD357B8331CC203E0345DFC19C73E6F1F70B6C2786E681D73BF48B15FE9992",
	"61BCF748BB05A48861578B8CB1855200B2E62A40E126BD7323E5B714645A54A2C8761EE39EE39BA6D2FE19B688168EDEA6DC5056400B5315ED299E7926176B887012E58634D78F05D7BCF0E1B81B1B41F5F8EF0B0711D3A64F9A317DD183AE039A4D3BE02A515892362F8C7BB6EB6434BB25418A438ED33D50C475122CBBE862",
	"7DB8D69D1605D9812B7F2F3E92BCEEB3426FEEE3265A174D71B2B6E16B332B43DF0B3C2FA152E48DE2FAC110D8CECE122C3398558E7987B27CACE12722C0032AC7E7766A9BCC881BA35B9DB9E751BD4E51F7683DE092F6C1D4DD937CDCE9C16E6F7D77CC6AAD806E4082E8E22E28592C4D78256354393FE831E811E03ED0A81A"
    },
    {
	"60C18B62F786DE6A4A8B13EB6DA2380B4C6731F861C715D9496DCF4A9F01CD33DDB52F1AB4D1F820FAF7AD4EFEB66586F7F08135714B13D77FE652B9EEAB2C543596A9ED307C1629CF535DD14AB22F081AE4ADF7A3E0BC7B33E0EC7A7306F9A737F55807974B5E1B7B6394BD0373917128B43A17757B34BAE1B600763E957F75",
	"0DEDA337C38EA005D5B8567EAB681CE91892C2C62C9D42BF748FBFE681E11F25D98280E42E1539A10EEE9177EF2F40216987936AF19D9B5EBE22EEAC27242D77CE3A5061F2E5CFACF15CD0F80E736AE8642252FE91E129DE3C78CFB85A0B1BB87B059CBB24483444F8A07244F4E89370BA78D58BD409DFBB3D41921B8879B9C7",
	"462C0707CF3366C2242A808CFDB79B77E8B3AF9D796583EB9CCD7BF4E8792AB0A818E49FFE53CA241F56988F825B366BF1E78481F8086A123259B9D83AC643E85845BF6B2C5412FFDDFAA8C9ED203CA4B3C1BFD777286099976472FA15B3CCC8418CF162F03C0C3E85D7EFC5CF5ACB9B2C039CCF3A1A9C6BB6B9C09C18D86CBD",
	"56DB382EDB8C2D95934D20261CE1A37090B0802D451E647DB1DA3B73CDB5A878EAD598A8817302449370F9D45E34F5C45F73D02BF4EB2B3712A8665F446F5D2B774039E5444AB74807859FA58DF9EBA4B12BA4545ACED827E4ED64CC71F937D64A1033BC43403F2490C1B715A74822B8D50A72A102213F0CF7A1B98B771B34C4"
    },
    {
	"61B7321207F4A73646E43E99221F902D2F38095E84CE7346A1510FE71BA7B9B34DCB6609E4DDDA8C82426E82D1C23F1E761130ECE4638D77554A7618E1608625049328FCC1F8845CA9A88E847106B01BD31EF6500E3C7EE81A048924BEAA3EDF367E5F4575341206C7A76427571898294B07BD918D4C2642854CC89D439042E5",
	"29AA38E63E4DD7C651E25DEC7A5A53E48114F52813793D36A9DBDD4F7C06FC38406E330764E0B2AFD811C39D857EA5F904105360E06856DC0780C7D61C53165833F0AEA15CB54732DE113F44C8FCFB86F4A876DD42D7A55356D91C0173F2B012680FB54C13EF54B65DF4AEDE2E13419B1316435187CEF07D44DB3DF57C4703FD",
	"5ED5AFB04CBFEE43EF3D9B60A57080831563648A2380D98F1EA4A96CF153903A40A2E564DED87E7254DF3270568AB952BF6F400681DD6AD919C9B06AC0F45F0646BCF37B217191AA0B7B7BED226B61F48B46DEA2E5A09E41F316583823A38A60FFD79085F43F60D98871ECA1A0F667701425094E88885A81DE9DA6C293E95060",
	"4DE4F24EAA3E2790FBCB1B13C2ED0EFD846EC33154DBEBBEFD895E1399B3617D55EC2CE8D71CF380B55D93636FEF741328D6B1E224D46F8A8B60A41D08DD86E88DE806AA781791364E6D88BF68571BF5D8C35CB04BA302227B7E4CB6A67AB7510ACBCDBF2F8A95EB5DEE693CCA5CC425A0F1CA2D18C369A767906A2477E32704"
    }
};

static void
dh_test(DH *server, DH *client)
{
    void *skey, *ckey;
    int ssize, csize;

    skey = emalloc(DH_size(server));
    ckey = emalloc(DH_size(client));

    ssize = DH_compute_key(skey, client->pub_key, server);
    if (ssize == -1)
	errx(1, "DH_compute_key failed for server");
    csize = DH_compute_key(ckey, server->pub_key, client);
    if (csize == -1)
	errx(1, "DH_compute_key failed for client");

    if (ssize != csize)
	errx(1, "DH_compute_key size mismatch");

    if (memcmp(skey, ckey, csize) != 0)
	errx(1, "DH_compute_key key mismatch");

    free(skey);
    free(ckey);
}


static int version_flag;
static int help_flag;
static char *id_flag;
static char *rsa_flag;
static int dh_flag = 1;
static int test_random_flag;

static struct getargs args[] = {
    { "id",		0,	arg_string,	&id_flag,
      "selects the engine id", 	"engine-id" },
    { "rsa",		0,	arg_string,	&rsa_flag,
      "tests RSA modes", 	"private-rsa-der-file" },
    { "dh",		0,	arg_negative_flag,	&dh_flag,
      "test dh", NULL },
    { "test-random",	0,	arg_flag,	&test_random_flag,
      "test if there is a random device", NULL },
    { "version",	0,	arg_flag,	&version_flag,
      "print version", NULL },
    { "help",		0,	arg_flag,	&help_flag,
      NULL, 	NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "filename.so");
    exit (ret);
}

int
main(int argc, char **argv)
{
    ENGINE *engine = NULL;
    int idx = 0;
    int have_rsa, have_dh;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &idx))
	usage(1);

    if (help_flag)
	usage(0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= idx;
    argv += idx;

    OpenSSL_add_all_algorithms();

    if (argc == 0) {
	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();
	engine = ENGINE_by_id("builtin");
    } else {
	engine = ENGINE_by_dso(argv[0], id_flag);
    }
    if (engine == NULL)
	errx(1, "ENGINE_by_dso failed");

    printf("name: %s\n", ENGINE_get_name(engine));
    printf("id: %s\n", ENGINE_get_id(engine));
    have_rsa = ENGINE_get_RSA(engine) != NULL;
    have_dh = ENGINE_get_DH(engine) != NULL;
    printf("RSA: %s", have_rsa ? "yes," : "no");
    if (have_rsa)
	printf(" %s", ENGINE_get_RSA(engine)->name);
    printf("\n");
    printf("DH: %s", have_dh ? "yes," : "no");
    if (have_dh)
	printf(" %s", ENGINE_get_DH(engine)->name);
    printf("\n");

    if (RAND_status() != 1)
	errx(77, "no functional random device, can't execute tests");
    if (test_random_flag)
	exit(0);

    if (rsa_flag && have_rsa) {
	unsigned char buf[1024 * 4];
	const unsigned char *p;
	size_t size;
	int keylen;
	RSA *rsa;
	FILE *f;

	f = fopen(rsa_flag, "rb");
	if (f == NULL)
	    err(1, "could not open file %s", rsa_flag);

	size = fread(buf, 1, sizeof(buf), f);
	if (size == 0)
	    err(1, "failed to read file %s", rsa_flag);
	if (size == sizeof(buf))
	    err(1, "key too long in file %s!", rsa_flag);
	fclose(f);

	p = buf;
	rsa = d2i_RSAPrivateKey(NULL, &p, size);
	if (rsa == NULL)
	    err(1, "failed to parse key in file %s", rsa_flag);

	RSA_set_method(rsa, ENGINE_get_RSA(engine));

	/*
	 * try rsa signing
	 */

	memcpy(buf, "hejsan", 7);
	keylen = RSA_private_encrypt(7, buf, buf, rsa, RSA_PKCS1_PADDING);
	if (keylen <= 0)
	    errx(1, "failed to private encrypt");

	keylen = RSA_public_decrypt(keylen, buf, buf, rsa, RSA_PKCS1_PADDING);
	if (keylen <= 0)
	    errx(1, "failed to public decrypt");

	if (keylen != 7)
	    errx(1, "output buffer not same length: %d", (int)keylen);

	if (memcmp(buf, "hejsan", 7) != 0)
	    errx(1, "string not the same after decryption");

	/*
	 * try rsa encryption
	 */

	memcpy(buf, "hejsan", 7);
	keylen = RSA_public_encrypt(7, buf, buf, rsa, RSA_PKCS1_PADDING);
	if (keylen <= 0)
	    errx(1, "failed to public encrypt");

	keylen = RSA_private_decrypt(keylen, buf, buf, rsa, RSA_PKCS1_PADDING);
	if (keylen <= 0)
	    errx(1, "failed to private decrypt");

	if (keylen != 7)
	    errx(1, "output buffer not same length: %d", (int)keylen);

	if (memcmp(buf, "hejsan", 7) != 0)
	    errx(1, "string not the same after decryption");

	RSA_free(rsa);

	printf("rsa test passed\n");

    }

    if (dh_flag) {
	DH *server, *client;
	int i;

	/* RFC2412-MODP-group2 */
	const char *p =
	    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
	    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
	    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
	    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
	    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE65381"
	    "FFFFFFFF" "FFFFFFFF";
	const char *g = "02";

	/*
	 * Try generated keys
	 */

	for (i = 0; i < 10; i++) {
	    server = DH_new_method(engine);
	    client = DH_new_method(engine);

	    BN_hex2bn(&server->p, p);
	    BN_hex2bn(&client->p, p);
	    BN_hex2bn(&server->g, g);
	    BN_hex2bn(&client->g, g);

	    if (!DH_generate_key(server))
		errx(1, "DH_generate_key failed for server");
	    if (!DH_generate_key(client))
		errx(1, "DH_generate_key failed for client");

	    dh_test(server, client);

	    DH_free(server);
	    DH_free(client);
	}
	/*
	 * Try known result
	 */

	for (i = 0; i < sizeof(dhtests)/sizeof(dhtests[0]); i++) {

	    server = DH_new_method(engine);
	    client = DH_new_method(engine);

	    BN_hex2bn(&server->p, p);
	    BN_hex2bn(&client->p, p);
	    BN_hex2bn(&server->g, g);
	    BN_hex2bn(&client->g, g);

	    BN_hex2bn(&client->priv_key, dhtests[i].cpriv);
	    BN_hex2bn(&client->pub_key, dhtests[i].cpub);
	    BN_hex2bn(&server->priv_key, dhtests[i].spriv);
	    BN_hex2bn(&server->pub_key, dhtests[i].spub);

	    dh_test(server, client);

	    DH_free(server);
	    DH_free(client);
	}

	printf("DH test passed\n");
    }

    ENGINE_finish(engine);

    return 0;
}
