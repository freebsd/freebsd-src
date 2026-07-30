/*-
 * Copyright (c) 2026 Dustin L. Howett
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/ChainedDiagnosticConsumer.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm/Support/CommandLine.h>

#include <print>

using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

static llvm::cl::OptionCategory LaToolCategory("la-clang-tool options");
static llvm::cl::opt<std::string> Prefix("prefix",
    llvm::cl::desc("Project path prefix"),
    llvm::cl::init(""),
    llvm::cl::cat(LaToolCategory));
static bool IsGithubCiBuild{};

struct MemCmpNeedleHandler : public MatchFinder::MatchCallback {
	virtual void run(const MatchFinder::MatchResult &result) {
		auto needle = result.Nodes.getNodeAs<clang::StringLiteral>("needle");
		auto lenExpr = result.Nodes.getNodeAs<clang::IntegerLiteral>("length");

		/*
		 * This check uses [less than] rather than [less equal] because in some formats we want to verify the
		 * presence of the null terminator and in others we do not.
		 */
		if(lenExpr->getValue().ult(needle->getLength())) {
			auto &diag = result.Context->getDiagnostics();
			auto did = diag.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
			    "memcmp needle has length `%0', but is checked with length `%1'");
			clang::DiagnosticBuilder db = diag.Report(lenExpr->getLocation(), did);
			db.AddTaggedVal(needle->getLength(), clang::DiagnosticsEngine::ArgumentKind::ak_uint);
			db.AddTaggedVal(lenExpr->getValue().getSExtValue(),
			    clang::DiagnosticsEngine::ArgumentKind::ak_sint);
			db.AddSourceRange(clang::CharSourceRange::getCharRange(needle->getSourceRange()));
		}
	}
};

struct AssertUAFHandler : public MatchFinder::MatchCallback {
	virtual void run(const MatchFinder::MatchResult &result) {
		/*
		 * This code uses getFileLoc to specify locations disregarding macro expansion
		 * and to avoid Clang printing "macro expanded from here".
		 */
		auto &SM = *result.SourceManager;
		auto uaf = result.Nodes.getNodeAs<clang::DeclRefExpr>("uaf");
		auto assertion = result.Nodes.getNodeAs<clang::CallExpr>("assertion");
		auto freed = result.Nodes.getNodeAs<clang::CallExpr>("free");

		auto &diag = result.Context->getDiagnostics();
		{
			auto did = diag.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
			    "test assertion uses `%0' after free; prefer assertEqualInt");
			clang::DiagnosticBuilder db = diag.Report(SM.getFileLoc(uaf->getBeginLoc()), did);
			db.AddString(uaf->getNameInfo().getAsString());
			db << clang::FixItHint::CreateReplacement(
			    clang::SourceRange{SM.getFileLoc(assertion->getBeginLoc()),
			        SM.getFileLoc(uaf->getBeginLoc())},
			    "assertEqualInt(");
		}

		{
			auto did = diag.getCustomDiagID(clang::DiagnosticsEngine::Level::Note, "`%0' freed here");
			clang::DiagnosticBuilder db = diag.Report(freed->getBeginLoc(), did);
			db.AddString(uaf->getNameInfo().getAsString());
		}
	}
};

class GithubActionDiagnosticConsumer : public clang::DiagnosticConsumer {
	virtual void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel, const clang::Diagnostic &Info) {
		static constexpr const char *levels[] = {nullptr, "notice", "notice", "warning", "error", "error"};

		const auto level = levels[DiagLevel];
		if(!level)
			return;

		auto &SM = Info.getSourceManager();
		auto loc = Info.getLocation();
		loc = SM.getFileLoc(loc);

		auto file = SM.getFilename(loc);
		if(file.empty()) {
			return;
		}
		file.consume_front_insensitive(Prefix);
		while(file.consume_front("/")) {}

		bool lineInvalid = false;
		auto line = SM.getLineNumber(loc, &lineInvalid);
		if(lineInvalid) {
			return;
		}

		llvm::SmallVector<char> diagString;
		Info.FormatDiagnostic(diagString);

		std::print("::{0} file={1},line={2},title=Lint::{3}\n", level, std::string_view{file}, line,
		    std::string_view{diagString});
	}
};

int main(int argc, const char **argv) {
	auto exp = CommonOptionsParser::create(argc, argv, LaToolCategory);
	auto &optionsParser = exp.get();
	ClangTool tool(optionsParser.getCompilations(), optionsParser.getSourcePathList());

	if(const char *e = getenv("GITHUB_WORKSPACE")) {
		IsGithubCiBuild = true;

		if(Prefix.c_str()[0] == '\0') {
			Prefix.assign(e);
			Prefix.append("/");
		}
	}

	MatchFinder finder;

	/*
	 * Find every assertion_equal_mem that passes a string literal.
	 * --> assertEqualMem(foo, "literal", 7);
	 */
	MemCmpNeedleHandler memCmpHandler;
	finder.addMatcher(callExpr(callee(functionDecl(hasName("assertion_equal_mem"))),
	                      hasArgument(4, stringLiteral().bind("needle")),
	                      hasArgument(6, integerLiteral(anything()).bind("length"))),
	    &memCmpHandler);

	/*
	 * Find every assertion_equal_int whose call site contains a `free` function,
	 * and then passes the freed decl to assertion_equal_int as context.
	 * --> assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));
	 *                     ^-- must be the same as ----------^
	 */
	AssertUAFHandler uafHandler;
	finder.addMatcher(
	    callExpr(callee(functionDecl(hasName("assertion_equal_int"))),
	        hasArgument(4,
	            callExpr(callee(functionDecl(matchesName("archive.*_free$"))),
	                hasArgument(0, declRefExpr(to(varDecl(anything()).bind("freed")))))
	                .bind("free")),
	        hasArgument(6, declRefExpr(to(varDecl(declaresSameEntityAsBoundNode("freed")))).bind("uaf")))
	        .bind("assertion"),
	    &uafHandler);

	std::unique_ptr<clang::DiagnosticConsumer> diagnosticConsumer;

	if(IsGithubCiBuild) {
		/* Emit GitHub-formatted error messages by default when we are building in GitHub Actions */
		clang::DiagnosticOptions diagOptions;
		diagOptions.ShowColors = true;
		clang::TextDiagnosticPrinter defaultDiag(llvm::errs(), diagOptions, false);
		diagnosticConsumer = std::make_unique<clang::ChainedDiagnosticConsumer>(&defaultDiag,
		    std::make_unique<GithubActionDiagnosticConsumer>());
		tool.setDiagnosticConsumer(diagnosticConsumer.get());
	}

	return tool.run(newFrontendActionFactory(&finder).get());
}
