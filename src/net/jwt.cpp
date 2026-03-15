#include "src/net/net.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>


int generate_gcal_jwt(char* service_email, char* privkey, int out_size, char* out) {
	// claim format: { iss: service_email, scope: https://www.googleapis.com/auth/calendar, aud: https://oauth2.googleapis.com/token, exp: now, iat: now+3600 }
	// todo: move over to using proper library instead of shexec

	const char* header = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";   // {"alg":"RS256","typ":"JWT"}

	const uint32_t CLAIM_SIZE = 1024;
	char claims[CLAIM_SIZE] = { 0 };
	snprintf(
	    claims, CLAIM_SIZE,
	    "{\\\"iss\\\": \\\"%s\\\", \\\"scope\\\": \\\"https://www.googleapis.com/auth/calendar\\\", \\\"aud\\\": \\\"https://oauth2.googleapis.com/token\\\", \\\"exp\\\": %ld, \\\"iat\\\": %ld}",
	    service_email, time(NULL) + 3600, time(NULL)
	);

	const char* JWT_TMP_PATH = "/tmp/jwt_header_claim.dat";
	const char* JWT_SIG_PATH = "/tmp/jwt_header_claim.sig";
	const uint32_t GEN_TOKEN_BUFSIZE = 8192;
	char gen_token_buf[GEN_TOKEN_BUFSIZE] = { 0 };
	memset(gen_token_buf, 0, GEN_TOKEN_BUFSIZE);
	snprintf(
	    gen_token_buf, GEN_TOKEN_BUFSIZE,
	    "echo -n $(echo -n %s).$(echo -n %s | base64 | tr '+/' '-_' | tr -d '=') | tr -d ' ' > %s && echo -n $'%s' | openssl dgst -sha256 -sign /dev/stdin -out %s %s 2>/dev/null && echo -n $(cat %s).$(cat %s | base64 -w 0 | tr '+/' '-_' | tr -d '=')",
	    header, claims, JWT_TMP_PATH,
	    privkey, JWT_SIG_PATH, JWT_TMP_PATH,
	    JWT_TMP_PATH, JWT_SIG_PATH
	);

	FILE* tokengen_fp = popen(gen_token_buf, "r");
	if (!tokengen_fp) { return 1; }  // failed to run command
	fgets(out, out_size, tokengen_fp);
	pclose(tokengen_fp);
	
	 return 0;
}
