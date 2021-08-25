#pragma once

#include <AK/Result.h>
#include <AK/String.h>
#include <AK/StringView.h>

struct Token {
    struct Position {
        size_t line { 0 };
        size_t column { 0 };
    };
    struct Range {
        Position start;
        Position end;
    };
    enum class Type {
        Unknown,
        Identifier,
        Comment,
        MentionOpen,
        MentionClose,
        IndirectMentionOpen,
        OpenBrace,
        CloseBrace,
        OpenParen,
        CloseParen,
        OpenBracket,
        CloseBracket,
        Comma,
        Colon,
        Semicolon,
        String,
        Integer,
        Pipe,
        Equals,
        Dot,
        Eof,
    };

    String text;
    Type type { Type::Unknown };
    Range source_range;
};

struct LexError {
    String error;
    Token::Position where;
};

class Lexer {
public:
    Result<Token, LexError> next();
    Token::Position current_source_position() const { return m_current_source_position; }

    Token emit_token(Token::Type, String);

private:
    Token::Position m_current_source_position;
    Token::Position m_start_source_position;
    Optional<char> m_next_input;
    StringBuilder m_string_builder;

    enum State {
        Free,
        InIdentifier,
        CouldBeInComment,
        InComment,
        InString,
        InInteger,
        CouldBeInIndirectCommentMention,
    } m_state { Free };
};
